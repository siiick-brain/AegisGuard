import json
import os
import stat
import signal
import socket
import ssl
import subprocess
import tempfile
import time
import unittest
from pathlib import Path
from unittest.mock import patch


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def available_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def make_test_certificates(
    directory: Path,
) -> tuple[Path, Path, Path, Path, Path]:
    ca_key = directory / "ca.key"
    ca_cert = directory / "ca.pem"
    server_key = directory / "server.key"
    server_request = directory / "server.csr"
    server_cert = directory / "server.pem"
    admin_key = directory / "admin.key"
    admin_request = directory / "admin.csr"
    admin_cert = directory / "admin.pem"

    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(ca_key), "-out", str(ca_cert), "-days", "1",
            "-subj", "/CN=AegisGuard Test CA",
            "-addext", "basicConstraints=critical,CA:TRUE",
            "-addext", "keyUsage=critical,keyCertSign,cRLSign",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [
            "openssl", "req", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(server_key), "-out", str(server_request),
            "-subj", "/CN=localhost",
            "-addext",
            "subjectAltName=DNS:localhost,DNS:status.test,IP:127.0.0.1",
            "-addext", "extendedKeyUsage=serverAuth",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [
            "openssl", "x509", "-req", "-in", str(server_request),
            "-CA", str(ca_cert), "-CAkey", str(ca_key), "-CAcreateserial",
            "-out", str(server_cert), "-days", "1", "-sha256",
            "-copy_extensions", "copy",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [
            "openssl", "req", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(admin_key), "-out", str(admin_request),
            "-subj", "/CN=AegisGuard Test Admin",
            "-addext", "extendedKeyUsage=clientAuth",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    subprocess.run(
        [
            "openssl", "x509", "-req", "-in", str(admin_request),
            "-CA", str(ca_cert), "-CAkey", str(ca_key), "-CAserial",
            str(directory / "ca.srl"), "-out", str(admin_cert),
            "-days", "1", "-sha256", "-copy_extensions", "copy",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return ca_cert, server_cert, server_key, admin_cert, admin_key


def make_untrusted_client_certificate(directory: Path) -> tuple[Path, Path]:
    client_key = directory / "untrusted-admin.key"
    client_certificate = directory / "untrusted-admin.pem"
    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", str(client_key), "-out", str(client_certificate),
            "-days", "1", "-subj", "/CN=Untrusted AegisGuard Admin",
            "-addext", "extendedKeyUsage=clientAuth",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    return client_certificate, client_key


def https_get(port: int, ca_cert: Path, path: str,
              client_cert: Path | None = None,
              client_key: Path | None = None) -> tuple[int, bytes]:
    context = ssl.create_default_context(cafile=str(ca_cert))
    if client_cert and client_key:
        context.load_cert_chain(str(client_cert), str(client_key))
    with socket.create_connection(("127.0.0.1", port), timeout=10) as raw:
        with context.wrap_socket(raw, server_hostname="localhost") as stream:
            stream.sendall(
                f"GET {path} HTTP/1.1\r\nHost: localhost\r\n"
                "Connection: close\r\n\r\n".encode("ascii")
            )
            response = bytearray()
            while True:
                chunk = stream.recv(4096)
                if not chunk:
                    break
                response.extend(chunk)
    header, _, body = response.partition(b"\r\n\r\n")
    status_line = header.split(b"\r\n", 1)[0].decode("ascii")
    return int(status_line.split()[1]), body


def assert_private_token_permissions(
    test_case: unittest.TestCase, token_path: Path
) -> None:
    if os.name == "nt":
        command = r"""
$path = $env:AEGISGUARD_TOKEN_ACL_PATH
$acl = Get-Acl -LiteralPath $path
$userSid = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$rules = @($acl.Access)
if (-not $acl.AreAccessRulesProtected -or $rules.Count -ne 1) {
    $entries = ($rules | ForEach-Object {
        "$($_.IdentityReference.Value):$($_.FileSystemRights):inherited=$($_.IsInherited)"
    }) -join "; "
    throw "Token ACL protected=$($acl.AreAccessRulesProtected) rules=$($rules.Count): $entries"
}
$rule = $rules[0]
$ruleSid = $rule.IdentityReference.Translate(
    [System.Security.Principal.SecurityIdentifier]
).Value
if ($ruleSid -ne $userSid -or
    $rule.AccessControlType -ne
        [System.Security.AccessControl.AccessControlType]::Allow -or
    $rule.FileSystemRights -ne
        [System.Security.AccessControl.FileSystemRights]::FullControl) {
    throw "Token ACL rule is not current-user full control."
}
"""
        result = subprocess.run(
            ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
             command],
            capture_output=True,
            text=True,
            env={
                **os.environ,
                "AEGISGUARD_TOKEN_ACL_PATH": str(token_path),
            },
            timeout=10,
        )
        test_case.assertEqual(
            result.returncode, 0, result.stdout + result.stderr
        )
        return

    mode = stat.S_IMODE(token_path.stat().st_mode)
    test_case.assertEqual(mode, stat.S_IRUSR | stat.S_IWUSR)


class ServerSmokeTests(unittest.TestCase):
    def build_directory(self) -> Path:
        configured_build = os.environ.get("AEGISGUARD_BUILD_DIR")
        if not configured_build:
            self.fail(
                "AEGISGUARD_BUILD_DIR must be set to the active CMake build directory"
            )
        build_directory = Path(configured_build)
        if not build_directory.is_dir():
            self.fail(
                f"AEGISGUARD_BUILD_DIR does not exist: {build_directory}"
            )
        return build_directory

    def require_binary(self, relative_path: Path) -> Path:
        binary = self.build_directory() / relative_path
        if not binary.is_file():
            self.fail(f"required test binary is missing: {binary}")
        return binary

    def assert_server_binary_is_current(self, server: Path) -> None:
        source_directory = REPOSITORY_ROOT / "server" / "src"
        source_files = [path for path in source_directory.rglob("*") if path.is_file()]
        if not source_files:
            self.fail(f"server source directory contains no files: {source_directory}")
        newest_source = max(source_files, key=lambda path: path.stat().st_mtime_ns)
        if server.stat().st_mtime_ns < newest_source.stat().st_mtime_ns:
            self.fail(
                "server binary is older than server source: "
                f"{server} is older than {newest_source}; rebuild the active "
                "AEGISGUARD_BUILD_DIR"
            )

    def test_build_directory_is_required(self) -> None:
        with patch.dict(os.environ):
            os.environ.pop("AEGISGUARD_BUILD_DIR", None)
            with self.assertRaisesRegex(
                AssertionError, "AEGISGUARD_BUILD_DIR must be set"
            ):
                self.build_directory()

    def test_missing_binary_fails_instead_of_skipping(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            with patch.dict(os.environ, {"AEGISGUARD_BUILD_DIR": directory}):
                with self.assertRaisesRegex(
                    AssertionError, "required test binary is missing"
                ):
                    self.require_binary(Path("missing-server"))

    def test_rejects_server_binary_older_than_source(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "server"
            binary.write_bytes(b"test")
            os.utime(binary, (1, 1))

            with self.assertRaisesRegex(
                AssertionError, "server binary is older than server source"
            ):
                self.assert_server_binary_is_current(binary)

    def test_valid_token_admin_mtls_and_signal_shutdown(self) -> None:
        binary_name = (
            "aegisguard_server.exe" if os.name == "nt" else "aegisguard_server"
        )
        server = self.require_binary(Path("server") / binary_name)
        self.assert_server_binary_is_current(server)

        with tempfile.TemporaryDirectory(prefix="aegisguard-smoke-") as temp:
            directory = Path(temp)
            ca_cert, server_cert, server_key, admin_cert, admin_key = \
                make_test_certificates(directory)
            untrusted_admin_cert, untrusted_admin_key = \
                make_untrusted_client_certificate(directory)
            token_path = directory / "player.token"
            config_path = directory / "server.json"
            game_port = available_port()
            admin_port = available_port()
            while admin_port == game_port:
                admin_port = available_port()
            config_path.write_text(json.dumps({
                "bind_address": "127.0.0.1",
                "port": game_port,
                "admin_port": admin_port,
                "tls_certificate": str(server_cert),
                "tls_private_key": str(server_key),
                "admin_ca_certificate": str(ca_cert),
                "token_file": str(token_path),
                "demo_player_id": 7,
                "initial_x": 10,
                "initial_y": 10,
            }), encoding="utf-8")

            creation_flags = 0
            popen_options: dict[str, object] = {}
            if os.name == "nt":
                creation_flags = subprocess.CREATE_NEW_PROCESS_GROUP
                popen_options["creationflags"] = creation_flags
            else:
                popen_options["start_new_session"] = True
            process = subprocess.Popen(
                [str(server), "--config", str(config_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                **popen_options,
            )
            output = ""
            try:
                deadline = time.monotonic() + 12
                health_status = None
                last_error: Exception | None = None
                while time.monotonic() < deadline:
                    if process.poll() is not None:
                        output = process.communicate(timeout=2)[0]
                        self.fail(f"server exited early ({process.returncode}): {output}")
                    try:
                        health_status, _ = https_get(admin_port, ca_cert, "/healthz")
                        break
                    except (OSError, ssl.SSLError) as error:
                        last_error = error
                        time.sleep(0.1)
                self.assertEqual(health_status, 200, f"health failed: {last_error}")
                assert_private_token_permissions(self, token_path)
                print("Demo token file permissions: owner only")
                healthcheck_command = [
                    str(server), "--config", str(config_path), "--healthcheck"
                ]
                hostname_environment = os.environ.copy()
                hostname_environment["AEGISGUARD_STATUS_HOSTNAME"] = "status.test"
                hostname_check = subprocess.run(
                    healthcheck_command,
                    capture_output=True,
                    text=True,
                    env=hostname_environment,
                    timeout=5,
                )
                self.assertEqual(
                    hostname_check.returncode,
                    0,
                    hostname_check.stdout + hostname_check.stderr,
                )
                hostname_environment["AEGISGUARD_STATUS_HOSTNAME"] = "wrong.test"
                mismatched_hostname_check = subprocess.run(
                    healthcheck_command,
                    capture_output=True,
                    text=True,
                    env=hostname_environment,
                    timeout=5,
                )
                self.assertNotEqual(mismatched_hostname_check.returncode, 0)
                print(
                    "status hostname check: status.test accepted; "
                    "certificate mismatch rejected"
                )
                self.assertEqual(https_get(admin_port, ca_cert, "/status")[0], 401)
                self.assertEqual(
                    https_get(admin_port, ca_cert, "/status", admin_cert, admin_key)[0],
                    200,
                )
                try:
                    untrusted_status = https_get(
                        admin_port, ca_cert, "/status",
                        untrusted_admin_cert, untrusted_admin_key
                    )[0]
                except (OSError, ssl.SSLError):
                    untrusted_status = None
                self.assertIn(untrusted_status, (None, 401))
                print("mTLS certificate check: trusted=200 untrusted=rejected")
                self.assertEqual(https_get(admin_port, ca_cert, "/metrics")[0], 401)
                metrics_status, metrics_body = https_get(
                    admin_port, ca_cert, "/metrics", admin_cert, admin_key
                )
                self.assertEqual(metrics_status, 200)
                self.assertEqual(metrics_body, b"aegisguard_up 1\n")

                token = bytes.fromhex(token_path.read_text(encoding="ascii").strip())
                self.assertEqual(len(token), 32)
                hello = bytearray(b"AG\x02\x00\x00\x02")
                hello.extend(bytes(8))
                hello.extend(b"\x00\x02")
                frame = bytearray(b"AG\x02\x01\x00\x0e")
                frame.extend((1).to_bytes(8, "big"))
                frame.extend((1000).to_bytes(2, "big", signed=True))
                frame.extend((0).to_bytes(2, "big", signed=True))
                frame.extend(bytes(2))
                frame.extend((10000).to_bytes(4, "big", signed=True))
                frame.extend((10000).to_bytes(4, "big", signed=True))
                self.assertEqual(len(hello), 16)
                self.assertEqual(len(frame), 28)

                game_context = ssl.create_default_context(cafile=str(ca_cert))
                with socket.create_connection(("127.0.0.1", game_port), timeout=3) as raw:
                    with game_context.wrap_socket(raw, server_hostname="localhost") as game:
                        game.sendall(token + hello + frame)
                        game.settimeout(0.3)
                        try:
                            self.assertNotEqual(game.recv(1), b"")
                        except socket.timeout:
                            pass

                if os.name == "nt":
                    process.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    process.send_signal(signal.SIGINT)
                output = process.communicate(timeout=8)[0]
                self.assertEqual(process.returncode, 0, output)
                server_lines = [line for line in output.splitlines() if line.strip()]
                server_logs = [json.loads(line) for line in server_lines]
                self.assertTrue(all(
                    isinstance(item, dict)
                    and isinstance(item.get("timestamp_ms"), int)
                    and isinstance(item.get("level"), str)
                    and isinstance(item.get("event"), str)
                    and isinstance(item.get("message"), str)
                    for item in server_logs
                ))
                self.assertFalse(any(
                    any(word in key.lower() for word in (
                        "token", "password", "secret", "private", "credential", "key"
                    ))
                    for item in server_logs for key in item
                ))
                self.assertNotIn(
                    token_path.read_text(encoding="ascii").strip(), output
                )
                self.assertNotIn(server_key.read_text(encoding="ascii"), output)
                print(
                    "Server JSON logs: "
                    f"lines={len(server_lines)} invalid=0 sensitive_fields=[]"
                )
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait(timeout=5)

    def test_json_logs_parse_and_exclude_secrets(self) -> None:
        probe_name = (
            "aegisguard_json_log_probe.exe"
            if os.name == "nt"
            else "aegisguard_json_log_probe"
        )
        probe = self.require_binary(Path("tests") / probe_name)

        result = subprocess.run(
            [str(probe)], check=True, capture_output=True, text=True, timeout=10
        )
        lines = [line for line in result.stderr.splitlines() if line.strip()]
        parsed = [json.loads(line) for line in lines]
        invalid_shape = [
            item for item in parsed
            if not isinstance(item, dict)
            or not isinstance(item.get("timestamp_ms"), int)
            or not isinstance(item.get("level"), str)
            or not isinstance(item.get("event"), str)
            or not isinstance(item.get("message"), str)
        ]
        sensitive_names = {
            key
            for item in parsed
            if isinstance(item, dict)
            for key in item
            if any(word in key.lower() for word in (
                "token", "password", "secret", "private", "credential", "key"
            ))
        }
        self.assertEqual(len(lines), 100)
        self.assertEqual(invalid_shape, [])
        self.assertEqual(sensitive_names, set())
        self.assertNotIn("secret", result.stderr.lower())
        print(
            "JSON log validation: "
            f"lines={len(lines)} invalid=0 sensitive_fields={sorted(sensitive_names)}"
        )


if __name__ == "__main__":
    unittest.main()
#include "aegisguard/network_service.hpp"
#include "aegisguard/json_log.hpp"
#include "aegisguard/server_config.hpp"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef _WIN32
#include <windows.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cerrno>
#include <cstddef>
#include <limits>
#include <system_error>
#include <vector>

#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using Tcp = asio::ip::tcp;
using namespace std::chrono_literals;

struct CommandLine {
    std::string config_path{"config/demo.json"};
    bool healthcheck{};
    bool help{};
};

CommandLine parse_command_line(int argc, char** argv) {
    CommandLine result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument{argv[index]};
        if (argument == "--config") {
            if (++index >= argc) {
                throw std::invalid_argument("--config requires a path");
            }
            result.config_path = argv[index];
        } else if (argument == "--healthcheck") {
            result.healthcheck = true;
        } else if (argument == "--help") {
            result.help = true;
        } else {
            throw std::invalid_argument("unknown argument: " + std::string(argument));
        }
    }
    return result;
}

std::string load_text_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open configured file: " + path);
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        throw std::runtime_error("could not read configured file: " + path);
    }
    return contents.str();
}

aegisguard::ServerConfig load_config(const std::string& path) {
    return aegisguard::parse_server_config(load_text_file(path));
}

void configure_tls13(asio::ssl::context& context) {
    SSL_CTX* native = context.native_handle();
    if (SSL_CTX_set_min_proto_version(native, TLS1_3_VERSION) != 1 ||
        SSL_CTX_set_max_proto_version(native, TLS1_3_VERSION) != 1) {
        throw std::runtime_error("could not restrict TLS to version 1.3");
    }
    constexpr auto options = SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION;
    if ((SSL_CTX_set_options(native, options) & options) != options) {
        throw std::runtime_error("could not set TLS context options");
    }
}

std::shared_ptr<asio::ssl::context> make_server_tls(
    const aegisguard::ServerConfig& config, bool require_client_certificate) {
    auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_server);
    configure_tls13(*context);
    context->use_certificate_chain_file(config.tls_certificate);
    context->use_private_key_file(config.tls_private_key, asio::ssl::context::pem);
    if (require_client_certificate) {
        if (config.admin_ca_certificate.empty()) {
            throw std::invalid_argument("admin_ca_certificate is required");
        }
        context->load_verify_file(config.admin_ca_certificate);
        context->set_verify_mode(asio::ssl::verify_peer);
    }
    return context;
}

std::string token_hex(const aegisguard::SessionCredential& credential) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(credential.size() * 2);
    for (const std::uint8_t value : credential) {
        result.push_back(digits[value >> 4U]);
        result.push_back(digits[value & 0x0FU]);
    }
    return result;
}

void write_demo_token(const std::string& path,
                      const aegisguard::SessionCredential& credential) {
    const std::filesystem::path token_path(path);
    if (!token_path.parent_path().empty()) {
        std::filesystem::create_directories(token_path.parent_path());
    }
    const std::string contents = token_hex(credential) + '\n';

#ifdef _WIN32
    class ScopedHandle {
    public:
        explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
        ~ScopedHandle() {
            if (handle_ != INVALID_HANDLE_VALUE) {
                CloseHandle(handle_);
            }
        }
        ScopedHandle(const ScopedHandle&) = delete;
        ScopedHandle& operator=(const ScopedHandle&) = delete;
        [[nodiscard]] HANDLE get() const { return handle_; }

    private:
        HANDLE handle_;
    };
    class ScopedLocalMemory {
    public:
        explicit ScopedLocalMemory(HLOCAL memory) : memory_(memory) {}
        ~ScopedLocalMemory() {
            if (memory_ != nullptr) {
                LocalFree(memory_);
            }
        }
        ScopedLocalMemory(const ScopedLocalMemory&) = delete;
        ScopedLocalMemory& operator=(const ScopedLocalMemory&) = delete;
        [[nodiscard]] HLOCAL get() const { return memory_; }

    private:
        HLOCAL memory_;
    };

    HANDLE process_token{};
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token)) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not open process token");
    }
    ScopedHandle token_handle(process_token);

    DWORD token_user_size = 0;
    SetLastError(ERROR_SUCCESS);
    GetTokenInformation(
        token_handle.get(), TokenUser, nullptr, 0, &token_user_size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || token_user_size == 0) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not size process user information");
    }
    std::vector<std::byte> token_user_data(token_user_size);
    if (!GetTokenInformation(
            token_handle.get(), TokenUser, token_user_data.data(),
            token_user_size, &token_user_size)) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not read process user information");
    }
    const auto* const token_user =
        reinterpret_cast<const TOKEN_USER*>(token_user_data.data());

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions = FILE_ALL_ACCESS;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance = NO_INHERITANCE;
    access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    access.Trustee.TrusteeType = TRUSTEE_IS_USER;
    access.Trustee.ptstrName =
        reinterpret_cast<LPWSTR>(token_user->User.Sid);
    PACL user_dacl = nullptr;
    const DWORD acl_result = SetEntriesInAclW(1, &access, nullptr, &user_dacl);
    if (acl_result != ERROR_SUCCESS) {
        throw std::system_error(
            static_cast<int>(acl_result), std::system_category(),
            "could not create token-file ACL");
    }
    ScopedLocalMemory dacl_memory(user_dacl);

    SECURITY_DESCRIPTOR security_descriptor{};
    if (!InitializeSecurityDescriptor(
            &security_descriptor, SECURITY_DESCRIPTOR_REVISION) ||
        !SetSecurityDescriptorDacl(
            &security_descriptor, TRUE, user_dacl, FALSE) ||
        !SetSecurityDescriptorControl(
            &security_descriptor, SE_DACL_PROTECTED, SE_DACL_PROTECTED)) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not prepare token-file security descriptor");
    }
    SECURITY_ATTRIBUTES security_attributes{
        sizeof(SECURITY_ATTRIBUTES), &security_descriptor, FALSE};
    const HANDLE raw_file = CreateFileW(
        token_path.c_str(), GENERIC_WRITE | WRITE_DAC, 0,
        &security_attributes, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (raw_file == INVALID_HANDLE_VALUE) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not create configured token file");
    }
    ScopedHandle file_handle(raw_file);

    const DWORD security_result = SetSecurityInfo(
        file_handle.get(), SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        nullptr, nullptr, user_dacl, nullptr);
    if (security_result != ERROR_SUCCESS) {
        throw std::system_error(
            static_cast<int>(security_result), std::system_category(),
            "could not restrict configured token file");
    }
    LARGE_INTEGER beginning{};
    if (!SetFilePointerEx(file_handle.get(), beginning, nullptr, FILE_BEGIN) ||
        !SetEndOfFile(file_handle.get())) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not prepare configured token file");
    }
    DWORD bytes_written = 0;
    if (contents.size() > std::numeric_limits<DWORD>::max() ||
        !WriteFile(file_handle.get(), contents.data(),
                   static_cast<DWORD>(contents.size()), &bytes_written, nullptr) ||
        bytes_written != contents.size() ||
        !FlushFileBuffers(file_handle.get())) {
        throw std::system_error(
            static_cast<int>(GetLastError()), std::system_category(),
            "could not write configured token file");
    }
#else
    int flags = O_WRONLY | O_CREAT;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif
    const int raw_file = ::open(
        token_path.c_str(), flags, S_IRUSR | S_IWUSR);
    if (raw_file == -1) {
        throw std::system_error(
            errno, std::system_category(), "could not create configured token file");
    }
    class ScopedFileDescriptor {
    public:
        explicit ScopedFileDescriptor(int descriptor) : descriptor_(descriptor) {}
        ~ScopedFileDescriptor() {
            if (descriptor_ >= 0) {
                ::close(descriptor_);
            }
        }
        ScopedFileDescriptor(const ScopedFileDescriptor&) = delete;
        ScopedFileDescriptor& operator=(const ScopedFileDescriptor&) = delete;
        [[nodiscard]] int get() const { return descriptor_; }

    private:
        int descriptor_;
    };
    ScopedFileDescriptor file(raw_file);
    if (::fchmod(file.get(), S_IRUSR | S_IWUSR) != 0 ||
        ::ftruncate(file.get(), 0) != 0) {
        throw std::system_error(
            errno, std::system_category(), "could not restrict configured token file");
    }
    std::size_t offset = 0;
    while (offset < contents.size()) {
        const ssize_t written = ::write(
            file.get(), contents.data() + offset, contents.size() - offset);
        if (written < 0 && errno == EINTR) {
            continue;
        }
        if (written <= 0) {
            throw std::system_error(
                errno, std::system_category(), "could not write configured token file");
        }
        offset += static_cast<std::size_t>(written);
    }
#endif
}

class AdminConnection final : public std::enable_shared_from_this<AdminConnection> {
public:
    AdminConnection(Tcp::socket socket,
                    std::shared_ptr<asio::ssl::context> tls_context)
        : stream_(std::move(socket), *tls_context),
          timeout_(stream_.get_executor()),
          request_(8192) {}

    void start() {
        timeout_.expires_after(3s);
        const auto self = shared_from_this();
        timeout_.async_wait([self](const asio::error_code& error) {
            if (!error) {
                self->close();
            }
        });
        stream_.async_handshake(asio::ssl::stream_base::server,
            [self](const asio::error_code& error) {
                if (error) {
                    self->close();
                    return;
                }
                X509* peer = SSL_get1_peer_certificate(self->stream_.native_handle());
                self->client_certificate_valid_ =
                    peer != nullptr && SSL_get_verify_result(
                        self->stream_.native_handle()) == X509_V_OK;
                if (peer) {
                    X509_free(peer);
                }
                asio::async_read_until(self->stream_, self->request_, "\r\n\r\n",
                    [self](const asio::error_code& read_error, std::size_t) {
                        if (read_error) {
                            self->close();
                            return;
                        }
                        self->handle_request();
                    });
            });
    }

private:
    asio::ssl::stream<Tcp::socket> stream_;
    asio::steady_timer timeout_;
    asio::streambuf request_;
    std::string response_;
    bool client_certificate_valid_{};
    bool closed_{};

    void handle_request() {
        aegisguard::write_json_log("info", "http_request_received");
        std::istream input(&request_);
        std::string method;
        std::string path;
        std::string version;
        input >> method >> path >> version;
        if (method != "GET" || (version != "HTTP/1.0" && version != "HTTP/1.1")) {
            respond("400 Bad Request", "text/plain", "bad request\n");
        } else if (path == "/healthz") {
            respond("200 OK", "application/json", "{\"status\":\"ok\"}\n");
        } else if (path == "/status") {
            if (!client_certificate_valid_) {
                respond("401 Unauthorized", "application/json",
                        "{\"error\":\"client certificate required\"}\n");
            } else {
                respond("200 OK", "application/json", "{\"status\":\"ok\"}\n");
            }
        } else if (path == "/metrics") {
            if (!client_certificate_valid_) {
                respond("401 Unauthorized", "text/plain",
                        "client certificate required\n");
            } else {
                respond("200 OK", "text/plain; version=0.0.4",
                        "aegisguard_up 1\n");
            }
        } else {
            respond("404 Not Found", "application/json", "{\"error\":\"not found\"}\n");
        }
    }

    void respond(std::string_view status, std::string_view content_type,
                 std::string_view body) {
        aegisguard::write_json_log("info", "http_response", status);
        response_ = "HTTP/1.1 " + std::string(status) + "\r\n" +
            "Content-Type: " + std::string(content_type) + "\r\n" +
            "Content-Length: " + std::to_string(body.size()) + "\r\n" +
            "Connection: close\r\n\r\n" + std::string(body);
        const auto self = shared_from_this();
        asio::async_write(stream_, asio::buffer(response_),
            [self](const asio::error_code&, std::size_t) { self->close(); });
    }

    void close() {
        if (closed_) {
            return;
        }
        closed_ = true;
        timeout_.cancel();
        asio::error_code ignored;
        stream_.lowest_layer().cancel(ignored);
        stream_.lowest_layer().shutdown(Tcp::socket::shutdown_both, ignored);
        stream_.lowest_layer().close(ignored);
    }
};

class AdminService final {
public:
    AdminService(asio::io_context& io, Tcp::endpoint endpoint,
                 std::shared_ptr<asio::ssl::context> tls_context)
        : io_(io), endpoint_(std::move(endpoint)), tls_context_(std::move(tls_context)),
          acceptor_(io) {}

    void start() {
        asio::error_code error;
        acceptor_.open(endpoint_.protocol(), error);
        if (error) throw asio::system_error(error, "open admin listener");
        acceptor_.set_option(Tcp::acceptor::reuse_address(true), error);
        if (error) throw asio::system_error(error, "configure admin listener");
        acceptor_.bind(endpoint_, error);
        if (error) throw asio::system_error(error, "bind admin listener");
        acceptor_.listen(asio::socket_base::max_listen_connections, error);
        if (error) throw asio::system_error(error, "listen on admin listener");
        accept_next();
    }

    void stop() {
        asio::error_code ignored;
        acceptor_.cancel(ignored);
        acceptor_.close(ignored);
    }

private:
    asio::io_context& io_;
    Tcp::endpoint endpoint_;
    std::shared_ptr<asio::ssl::context> tls_context_;
    Tcp::acceptor acceptor_;

    void accept_next() {
        auto socket = std::make_shared<Tcp::socket>(io_);
        acceptor_.async_accept(*socket,
            [this, socket](const asio::error_code& error) {
                if (!error) {
                    std::make_shared<AdminConnection>(
                        std::move(*socket), tls_context_)->start();
                    accept_next();
                }
            });
    }
};

int healthcheck(const aegisguard::ServerConfig& config) {
    try {
        asio::io_context io;
        asio::ssl::context tls(asio::ssl::context::tls_client);
        configure_tls13(tls);
        tls.load_verify_file(config.admin_ca_certificate);
        tls.set_verify_mode(asio::ssl::verify_peer);
        asio::ssl::stream<Tcp::socket> stream(io, tls);
        stream.set_verify_callback(asio::ssl::host_name_verification("localhost"));
        if (SSL_set_tlsext_host_name(stream.native_handle(), "localhost") != 1) {
            return 1;
        }
        stream.lowest_layer().connect(Tcp::endpoint(
            asio::ip::make_address("127.0.0.1"), config.admin_port));
        stream.handshake(asio::ssl::stream_base::client);
        asio::write(stream, asio::buffer(
            "GET /healthz HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n"));
        asio::streambuf response;
        asio::read_until(stream, response, "\r\n");
        std::istream input(&response);
        std::string status_line;
        std::getline(input, status_line);
        return status_line.find(" 200 ") != std::string::npos ? 0 : 1;
    } catch (...) {
        return 1;
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CommandLine arguments = parse_command_line(argc, argv);
        if (arguments.help) {
            std::cout << "Usage: aegisguard_server [--config PATH] [--healthcheck] [--help]\n";
            return 0;
        }
        const aegisguard::ServerConfig config = load_config(arguments.config_path);
        if (arguments.healthcheck) {
            return healthcheck(config);
        }
        if (config.tls_certificate.empty() || config.tls_private_key.empty() ||
            config.admin_ca_certificate.empty() || config.token_file.empty()) {
            throw std::invalid_argument(
                "tls_certificate, tls_private_key, admin_ca_certificate and token_file are required");
        }

        auto io = std::make_shared<asio::io_context>();
        auto game_tls = make_server_tls(config, false);
        auto admin_tls = make_server_tls(config, true);
        aegisguard::Simulation simulation(config.simulation);
        if (simulation.add_player(config.demo_player_id, config.initial_position) !=
            aegisguard::SimulationError::none) {
            throw std::invalid_argument("configured demo player is invalid");
        }
        aegisguard::SessionStore sessions(config.maximum_sessions);
        const auto issued = sessions.issue(config.demo_player_id,
            aegisguard::SessionStore::Duration{3600000},
            aegisguard::SessionStore::Clock::now());
        if (issued.error != aegisguard::SessionError::none) {
            throw std::runtime_error("could not issue demo session token");
        }
        write_demo_token(config.token_file, issued.credential);

        aegisguard::NetworkServiceConfig network_config;
        network_config.endpoint = Tcp::endpoint(
            asio::ip::make_address(config.bind_address), config.port);
        network_config.maximum_connections = config.maximum_connections;
        network_config.authentication_timeout = config.authentication_timeout;
        network_config.frame_timeout = config.frame_timeout;
        network_config.movement_rate_capacity = config.movement_rate_capacity;
        network_config.movement_rate_per_second = config.movement_rate_per_second;
        network_config.ip_rate_capacity = config.ip_rate_capacity;
        network_config.ip_rate_per_second = config.ip_rate_per_second;
        network_config.maximum_tracked_ip_rates =
            config.maximum_tracked_ip_rates;
        network_config.maximum_queued_intents_per_player =
            config.maximum_queued_intents_per_player;
        network_config.movement_anomaly.maximum_speed = config.simulation.max_speed;
        network_config.on_error = [](std::error_code error) {
            aegisguard::write_json_log(
                "error", "network_service_error", error.message());
        };
        network_config.on_detection = [](const aegisguard::RuleFinding& finding) {
            aegisguard::write_json_log(
                "warning", "rule_detection",
                aegisguard::rule_signal_description(finding.signal));
        };

        aegisguard::NetworkService game_service(
            io, game_tls, sessions, simulation, std::move(network_config));
        AdminService admin_service(*io, Tcp::endpoint(
            asio::ip::make_address(config.bind_address), config.admin_port), admin_tls);
        asio::signal_set signals(*io, SIGINT, SIGTERM);
    #ifdef SIGBREAK
        signals.add(SIGBREAK);
    #endif
        signals.async_wait([&](const asio::error_code& error, int) {
            if (!error) {
                game_service.stop();
                admin_service.stop();
            }
        });

        game_service.start();
        admin_service.start();
        aegisguard::write_json_log("info", "server_started", "listeners ready");
        io->run();
        aegisguard::write_json_log("info", "server_stopped", "shutdown complete");
        return 0;
    } catch (const std::exception& error) {
        aegisguard::write_json_log("error", "server_startup_failed", error.what());
        return 1;
    }
}
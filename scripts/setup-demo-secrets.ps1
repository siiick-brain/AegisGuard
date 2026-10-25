param(
    [string]$OutputDirectory = "secrets"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repositoryRoot

$opensslCommand = Get-Command openssl -ErrorAction Stop
$outputPath = Join-Path $repositoryRoot $OutputDirectory
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

function Invoke-OpenSsl([string[]]$Arguments) {
    & $opensslCommand.Source @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "openssl exited with ${LASTEXITCODE}"
    }
}

$caKey = Join-Path $outputPath "ca.key"
$caCertificate = Join-Path $outputPath "ca.pem"
$serverKey = Join-Path $outputPath "server.key"
$serverRequest = Join-Path $outputPath "server.csr"
$serverCertificate = Join-Path $outputPath "server.pem"
$adminKey = Join-Path $outputPath "admin.key"
$adminRequest = Join-Path $outputPath "admin.csr"
$adminCertificate = Join-Path $outputPath "admin.pem"

if (-not (Test-Path $caKey)) {
    Invoke-OpenSsl @("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", $caKey)
}
if (-not (Test-Path $caCertificate)) {
    Invoke-OpenSsl @("req", "-x509", "-new", "-key", $caKey, "-sha256", "-days", "3650", "-out", $caCertificate, "-subj", "/CN=AegisGuard Demo CA", "-addext", "basicConstraints=critical,CA:TRUE", "-addext", "keyUsage=critical,keyCertSign,cRLSign")
}

if (-not (Test-Path $serverKey)) {
    Invoke-OpenSsl @("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", $serverKey)
}
if (-not (Test-Path $serverRequest)) {
    Invoke-OpenSsl @("req", "-new", "-key", $serverKey, "-out", $serverRequest, "-subj", "/CN=localhost", "-addext", "subjectAltName=DNS:localhost,IP:127.0.0.1", "-addext", "extendedKeyUsage=serverAuth")
}
if (-not (Test-Path $serverCertificate)) {
    Invoke-OpenSsl @("x509", "-req", "-in", $serverRequest, "-CA", $caCertificate, "-CAkey", $caKey, "-CAcreateserial", "-out", $serverCertificate, "-days", "30", "-sha256", "-copy_extensions", "copy")
}

if (-not (Test-Path $adminKey)) {
    Invoke-OpenSsl @("genpkey", "-algorithm", "RSA", "-pkeyopt", "rsa_keygen_bits:2048", "-out", $adminKey)
}
if (-not (Test-Path $adminRequest)) {
    Invoke-OpenSsl @("req", "-new", "-key", $adminKey, "-out", $adminRequest, "-subj", "/CN=AegisGuard Demo Admin", "-addext", "extendedKeyUsage=clientAuth")
}
if (-not (Test-Path $adminCertificate)) {
    Invoke-OpenSsl @("x509", "-req", "-in", $adminRequest, "-CA", $caCertificate, "-CAkey", $caKey, "-CAserial", (Join-Path $outputPath "ca.srl"), "-out", $adminCertificate, "-days", "30", "-sha256", "-copy_extensions", "copy")
}

Invoke-OpenSsl @("verify", "-CAfile", $caCertificate, $serverCertificate, $adminCertificate)
Write-Output "Created demo CA, localhost server certificate and admin client certificate in ignored $OutputDirectory."
param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        throw "LOCALAPPDATA is unavailable; pass an absolute OutputDirectory."
    }
    $OutputDirectory = Join-Path $env:LOCALAPPDATA "AegisGuard\secrets"
}
if (-not [System.IO.Path]::IsPathRooted($OutputDirectory)) {
    throw "OutputDirectory must be an absolute path outside the repository."
}
$outputPath = [System.IO.Path]::GetFullPath($OutputDirectory)
$repositoryPath = [System.IO.Path]::GetFullPath($repositoryRoot).TrimEnd(
    [System.IO.Path]::DirectorySeparatorChar,
    [System.IO.Path]::AltDirectorySeparatorChar
)
$repositoryPrefix = $repositoryPath + [System.IO.Path]::DirectorySeparatorChar
if ($outputPath.Equals(
        $repositoryPath,
        [System.StringComparison]::OrdinalIgnoreCase
    ) -or $outputPath.StartsWith(
        $repositoryPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
    throw "OutputDirectory must be outside the repository."
}

$opensslCommand = Get-Command openssl -ErrorAction Stop
New-Item -ItemType Directory -Force -Path $outputPath | Out-Null

function Set-PrivateDirectoryPermissions([string]$Path) {
    if ($env:OS -eq "Windows_NT") {
        if (Test-PrivateAcl -Path $Path -Directory) {
            return
        }
        $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
        $acl = Get-Acl -LiteralPath $Path
        $acl.SetAccessRuleProtection($true, $false)
        foreach ($rule in @($acl.Access)) {
            [void]$acl.RemoveAccessRuleSpecific($rule)
        }
        $rule = [System.Security.AccessControl.FileSystemAccessRule]::new(
            $identity.User,
            [System.Security.AccessControl.FileSystemRights]::FullControl,
            [System.Security.AccessControl.InheritanceFlags]::ContainerInherit -bor
                [System.Security.AccessControl.InheritanceFlags]::ObjectInherit,
            [System.Security.AccessControl.PropagationFlags]::None,
            [System.Security.AccessControl.AccessControlType]::Allow
        )
        $acl.SetAccessRule($rule)
        Set-Acl -LiteralPath $Path -AclObject $acl
        if (-not (Test-PrivateAcl -Path $Path -Directory)) {
            throw "Private directory ACL is not restricted to the current user: $Path"
        }
        return
    }

    & chmod 700 $Path
    if ($LASTEXITCODE -ne 0) {
        throw "chmod failed for private directory: $Path"
    }
}

function Set-PrivateFilePermissions([string]$Path) {
    if ($env:OS -eq "Windows_NT") {
        if (Test-PrivateAcl -Path $Path) {
            return
        }
        $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent()
        $acl = Get-Acl -LiteralPath $Path
        $acl.SetAccessRuleProtection($true, $false)
        foreach ($rule in @($acl.Access)) {
            [void]$acl.RemoveAccessRuleSpecific($rule)
        }
        $rule = [System.Security.AccessControl.FileSystemAccessRule]::new(
            $identity.User,
            [System.Security.AccessControl.FileSystemRights]::FullControl,
            [System.Security.AccessControl.AccessControlType]::Allow
        )
        $acl.SetAccessRule($rule)
        Set-Acl -LiteralPath $Path -AclObject $acl
        if (-not (Test-PrivateAcl -Path $Path)) {
            throw "Private key ACL is not restricted to the current user: $Path"
        }
        return
    }

    & chmod 600 $Path
    if ($LASTEXITCODE -ne 0) {
        throw "chmod failed for private key: $Path"
    }
}

function Test-PrivateAcl(
    [string]$Path,
    [switch]$Directory
) {
    $identity = [System.Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $acl = Get-Acl -LiteralPath $Path
    if (-not $acl.AreAccessRulesProtected) {
        return $false
    }
    $rules = @($acl.Access)
    if ($rules.Count -ne 1) {
        return $false
    }
    $rule = $rules[0]
    $ruleIdentity = $rule.IdentityReference.Translate(
        [System.Security.Principal.SecurityIdentifier]
    ).Value
    if ($ruleIdentity -ne $identity -or
        $rule.AccessControlType -ne
            [System.Security.AccessControl.AccessControlType]::Allow -or
        $rule.FileSystemRights -ne
            [System.Security.AccessControl.FileSystemRights]::FullControl) {
        return $false
    }
    if ($Directory) {
        $expectedInheritance =
            [System.Security.AccessControl.InheritanceFlags]::ContainerInherit -bor
            [System.Security.AccessControl.InheritanceFlags]::ObjectInherit
        return $rule.InheritanceFlags -eq $expectedInheritance
    }
    return $rule.InheritanceFlags -eq
        [System.Security.AccessControl.InheritanceFlags]::None
}

Set-PrivateDirectoryPermissions $outputPath

function Invoke-OpenSsl([string[]]$Arguments) {
    $previousErrorActionPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(
            & $opensslCommand.Source @Arguments 2>&1 |
                ForEach-Object { $_.ToString() }
        )
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($exitCode -ne 0) {
        throw "openssl exited with ${exitCode}: $($output -join ' ')"
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
Set-PrivateFilePermissions $caKey
Set-PrivateFilePermissions $serverKey
Set-PrivateFilePermissions $adminKey
Write-Output "Verified demo certificates and keys. Private keys and their directory are restricted to the current user."
param(
    [string]$CommitMessagePath
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repositoryRoot

$files = @(
    git ls-files --cached --others --exclude-standard |
        Where-Object {
            $_ -match '\.(md|yml|yaml|ps1|py|cpp|cc|cxx|h|hpp|sh|cmake)$' -or
            $_ -match '(^|/)CMakeLists\.txt$' -or
            $_ -match '(^|/)Dockerfile$'
        } |
        Sort-Object -Unique
)
foreach ($privatePath in @(
    "private/PROGRESS.md",
    "scripts/hooks/commit-msg"
)) {
    if ((Test-Path -LiteralPath $privatePath -PathType Leaf) -and
        $files -notcontains $privatePath) {
        $files += $privatePath
    }
}
$failures = New-Object "System.Collections.Generic.List[string]"
$warnings = New-Object "System.Collections.Generic.List[string]"
$dashPattern = '[\u2013\u2014]'
$serialCommaPattern = '\b[\w-]+(?:\s+[\w-]+)?\s*,\s*[\w-]+(?:\s+[\w-]+)?\s*,\s+and\b'
$andCommaPattern = '\band\s*,'

function Invoke-GitDiffCheck([string[]]$Arguments) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(
            & git @Arguments 2>&1 |
                ForEach-Object { $_.ToString() }
        )
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = $output
    }
}

foreach ($relativePath in $files) {
    $path = Join-Path $repositoryRoot $relativePath.Replace('/', '\')
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        continue
    }
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $path) {
        $lineNumber++
        if ($line -match $dashPattern) {
            $failures.Add("${relativePath}:${lineNumber}: em or en dash")
        }
        if ($line -match $serialCommaPattern) {
            $failures.Add("${relativePath}:${lineNumber}: serial comma")
        }
        if ($line -match $andCommaPattern) {
            $failures.Add("${relativePath}:${lineNumber}: comma after and")
        }
    }
}

if (-not [string]::IsNullOrWhiteSpace($CommitMessagePath)) {
    if (-not (Test-Path -LiteralPath $CommitMessagePath -PathType Leaf)) {
        throw "Commit message file does not exist: $CommitMessagePath"
    }
    $lineNumber = 0
    foreach ($line in Get-Content -LiteralPath $CommitMessagePath) {
        $lineNumber++
        if ($line -match $dashPattern) {
            $failures.Add("${CommitMessagePath}:${lineNumber}: em or en dash")
        }
        if ($line -match $serialCommaPattern) {
            $failures.Add("${CommitMessagePath}:${lineNumber}: serial comma")
        }
        if ($line -match $andCommaPattern) {
            $failures.Add("${CommitMessagePath}:${lineNumber}: comma after and")
        }
    }
}

$diffCheck = Invoke-GitDiffCheck @("diff", "--check")
if ($diffCheck.ExitCode -ne 0) {
    $failures.Add("git diff --check failed: $($diffCheck.Output -join ' ')")
} elseif ($diffCheck.Output.Count -gt 0) {
    foreach ($line in $diffCheck.Output) {
        $warnings.Add("git diff --check: $line")
    }
}
$stagedCheck = Invoke-GitDiffCheck @("diff", "--cached", "--check")
if ($stagedCheck.ExitCode -ne 0) {
    $failures.Add("git diff --cached --check failed: $($stagedCheck.Output -join ' ')")
} elseif ($stagedCheck.Output.Count -gt 0) {
    foreach ($line in $stagedCheck.Output) {
        $warnings.Add("git diff --cached --check: $line")
    }
}

Write-Output "Writing style scan checked $($files.Count) files."
$warnings | ForEach-Object { Write-Output "WARN: $_" }
if ($failures.Count -gt 0) {
    $failures | ForEach-Object { Write-Output "FAIL: $_" }
    exit 1
}
Write-Output "PASS: no dash, comma or whitespace violations."

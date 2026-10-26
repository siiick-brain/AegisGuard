param(
    [string]$MutationManifest = "tests/mutation/mutations.json",
    [string]$BuildDir = "build-mutation"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repositoryRoot

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $repositoryRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$env:AEGISGUARD_BUILD_DIR = $BuildDir
$mutationPath = Join-Path $repositoryRoot $MutationManifest
$mutants = Get-Content -LiteralPath $mutationPath -Raw | ConvertFrom-Json
if ($mutants.Count -eq 0) {
    throw "Mutation manifest contains no mutants."
}
$results = New-Object "System.Collections.Generic.List[object]"
$utf8 = New-Object System.Text.UTF8Encoding($false)

function Invoke-NativeCapture(
    [string]$Command,
    [string[]]$Arguments
) {
    $previousPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = "Continue"
        $output = @(
            & $Command @Arguments 2>&1 |
                ForEach-Object { $_.ToString() }
        )
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousPreference
    }
    return [pscustomobject]@{
        ExitCode = $exitCode
        Output = ($output -join "`n")
    }
}

$configure = Invoke-NativeCapture "cmake" @(
    "-S", ".", "-B", $BuildDir, "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug", "-DBUILD_TESTING=ON"
)
if ($configure.ExitCode -ne 0) {
    throw "Mutation build configuration failed:`n$($configure.Output)"
}

foreach ($mutant in $mutants) {
    $relativePath = [string]$mutant.PSObject.Properties["file"].Value
    if ([string]::IsNullOrWhiteSpace($relativePath)) {
        throw "Mutation $($mutant.id) does not specify a file."
    }
    $path = Join-Path -Path $repositoryRoot -ChildPath $relativePath
    $originalBytes = [System.IO.File]::ReadAllBytes($path)
    $originalText = $utf8.GetString($originalBytes).Replace("`r`n", "`n")
    $matchCount = ([regex]::Matches(
        $originalText, [regex]::Escape($mutant.find)
    )).Count
    if ($matchCount -ne 1) {
        $results.Add([pscustomobject]@{
            Id = $mutant.id
            Category = $mutant.category
            Result = "Excluded"
            Note = "Mutation anchor matched $matchCount times, expected exactly once."
        })
        continue
    }

    try {
        $mutatedText = $originalText.Replace($mutant.find, $mutant.replace)
        [System.IO.File]::WriteAllText($path, $mutatedText, $utf8)

        if ($mutant.runner -eq "ctest") {
            $build = Invoke-NativeCapture "cmake" @(
                "--build", $BuildDir, "--parallel", "2",
                "--target", $mutant.target
            )
            if ($build.ExitCode -ne 0) {
                $results.Add([pscustomobject]@{
                    Id = $mutant.id
                    Category = $mutant.category
                    Result = "Excluded"
                    Note = "Mutant did not compile: $($build.Output)"
                })
                continue
            }
            $test = Invoke-NativeCapture "ctest" @(
                "--test-dir", $BuildDir, "--output-on-failure",
                "-R", $mutant.test
            )
        } elseif ($mutant.runner -eq "python") {
            if ($mutant.PSObject.Properties.Name -contains "buildTarget") {
                $build = Invoke-NativeCapture "cmake" @(
                    "--build", $BuildDir, "--parallel", "2",
                    "--target", $mutant.buildTarget
                )
                if ($build.ExitCode -ne 0) {
                    $results.Add([pscustomobject]@{
                        Id = $mutant.id
                        Category = $mutant.category
                        Result = "Excluded"
                        Note = "Mutant did not compile: $($build.Output)"
                    })
                    continue
                }
            }
            $test = Invoke-NativeCapture "python" @(
                "-m", "pytest", "-q", $mutant.target
            )
        } else {
            $results.Add([pscustomobject]@{
                Id = $mutant.id
                Category = $mutant.category
                Result = "Excluded"
                Note = "Unknown runner: $($mutant.runner)"
            })
            continue
        }

        if (($mutant.runner -eq "ctest" -and
             $test.Output -match "No tests were found") -or
            ($mutant.runner -eq "python" -and $test.ExitCode -eq 5)) {
            $results.Add([pscustomobject]@{
                Id = $mutant.id
                Category = $mutant.category
                Result = "Excluded"
                Note = "The configured test selection did not collect any tests."
            })
            continue
        }

        $result = if ($test.ExitCode -eq 0) { "Survived" } else { "Killed" }
        $results.Add([pscustomobject]@{
            Id = $mutant.id
            Category = $mutant.category
            Result = $result
            Note = $mutant.description
        })
    } finally {
        [System.IO.File]::WriteAllBytes($path, $originalBytes)
    }
}

$generated = $results.Count
$killed = @($results | Where-Object { $_.Result -eq "Killed" }).Count
$survived = @($results | Where-Object { $_.Result -eq "Survived" }).Count
$excluded = @($results | Where-Object { $_.Result -eq "Excluded" }).Count
$valid = $killed + $survived
$score = if ($valid -eq 0) { 0.0 } else { 100.0 * $killed / $valid }

New-Item -ItemType Directory -Force -Path "artifacts" | Out-Null
$report = [pscustomobject]@{
    Generated = $generated
    Killed = $killed
    Survived = $survived
    Excluded = $excluded
    MutationScorePercent = [math]::Round($score, 2)
    Results = $results.ToArray()
}
$report | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath "artifacts/mutation-report.json" -Encoding UTF8
$results | Format-Table -AutoSize
Write-Output "Generated: $generated"
Write-Output "Killed: $killed"
Write-Output "Survived: $survived"
Write-Output "Excluded: $excluded"
Write-Output "Mutation score: $([math]::Round($score, 2))%"

if ($survived -gt 0 -or $excluded -gt 0) {
    exit 1
}
exit 0

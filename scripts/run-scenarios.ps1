param(
    [string]$BuildDirectory = "build"
)

$ErrorActionPreference = "Stop"
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildPath = if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    [System.IO.Path]::GetFullPath($BuildDirectory)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $BuildDirectory))
}

if (-not (Test-Path -LiteralPath (Join-Path $buildPath "CTestTestfile.cmake"))) {
    throw "No configured test build found at $buildPath. Configure and build the project first."
}

$scenarioPattern = '^(NetworkServiceTest\.(ForgedPositionRejected|SpeedCheatRejected|TeleportCheatRejected|AimSnapCheatRejected)|SimulationTest\.(FireDamagesTheNearestServerOwnedTarget|RespawnsEliminatedPlayerAtItsServerOwnedSpawnPoint))$'
Write-Output "Running six existing server-authority scenarios."
& ctest --test-dir $buildPath --output-on-failure --no-tests=error -R $scenarioPattern
if ($LASTEXITCODE -ne 0) {
    throw "Scenario tests failed with exit code $LASTEXITCODE."
}

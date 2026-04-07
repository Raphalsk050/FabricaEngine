param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug",
  [string]$BuildDir = "engine/build",
  [switch]$Clean,
  [switch]$NoTests,
  [switch]$BuildSamples,
  [switch]$DisableGlfwBackend,
  [switch]$FetchGlfw,
  [string]$Target = "",
  [int]$Parallel = 0
)

$ErrorActionPreference = "Stop"

function To-OnOff {
  param([bool]$Value)
  if ($Value) { return "ON" }
  return "OFF"
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
  Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

$buildTests = -not $NoTests
$glfwBackend = -not $DisableGlfwBackend

$configureArgs = @(
  "-S", "engine",
  "-B", $BuildDir,
  "-DFABRICA_BUILD_TESTS=$(To-OnOff $buildTests)",
  "-DFABRICA_BUILD_SAMPLES=$(To-OnOff $BuildSamples.IsPresent)",
  "-DFABRICA_ENABLE_GLFW_WINDOW_BACKEND=$(To-OnOff $glfwBackend)",
  "-DFABRICA_FETCH_GLFW=$(To-OnOff $FetchGlfw.IsPresent)"
)

Write-Host "[build_windows] Configure: cmake $($configureArgs -join ' ')"
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$buildArgs = @("--build", $BuildDir, "--config", $Config)
if ($Parallel -gt 0) {
  $buildArgs += @("--parallel", $Parallel)
} else {
  $buildArgs += "--parallel"
}

if ($Target -ne "") {
  $buildArgs += @("--target", $Target)
}

Write-Host "[build_windows] Build: cmake $($buildArgs -join ' ')"
& cmake @buildArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($buildTests) {
  $testArgs = @("--test-dir", $BuildDir, "-C", $Config, "--output-on-failure")
  Write-Host "[build_windows] Test: ctest $($testArgs -join ' ')"
  & ctest @testArgs
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "[build_windows] Done"
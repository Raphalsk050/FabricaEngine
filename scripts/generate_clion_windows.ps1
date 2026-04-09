param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug",
  [string]$BuildDir = "engine/samples/cmake-build-debug",
  [string]$NinjaPath = "",
  [switch]$NoSamples,
  [switch]$WithTests,
  [switch]$DisableGlfwBackend,
  [switch]$NoFetchGlfw,
  [switch]$Build,
  [string]$Target = "fabrica_runtime_sample",
  [switch]$Clean,
  [int]$Parallel = 0
)

$ErrorActionPreference = "Stop"

function To-OnOff {
  param([bool]$Value)
  if ($Value) { return "ON" }
  return "OFF"
}

function Resolve-NinjaPath {
  param([string]$ExplicitPath)

  if ($ExplicitPath -ne "") {
    if (Test-Path -LiteralPath $ExplicitPath) {
      return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }
    throw "Provided Ninja path does not exist: $ExplicitPath"
  }

  $fromPath = Get-Command ninja -ErrorAction SilentlyContinue
  if ($fromPath -ne $null) {
    return $fromPath.Source
  }

  $candidatePaths = @(
    "$env:LOCALAPPDATA\Programs\CLion\bin\ninja\win\x64\ninja.exe",
    "$env:ProgramFiles\JetBrains\CLion\bin\ninja\win\x64\ninja.exe"
  )

  foreach ($candidate in $candidatePaths) {
    if ($candidate -and (Test-Path -LiteralPath $candidate)) {
      return (Resolve-Path -LiteralPath $candidate).Path
    }
  }

  $toolboxRoot = Join-Path $env:LOCALAPPDATA "JetBrains\Toolbox\apps\CLion\ch-0"
  if (Test-Path -LiteralPath $toolboxRoot) {
    $channels = Get-ChildItem -LiteralPath $toolboxRoot -Directory | Sort-Object Name -Descending
    foreach ($channel in $channels) {
      $candidate = Join-Path $channel.FullName "bin\ninja\win\x64\ninja.exe"
      if (Test-Path -LiteralPath $candidate) {
        return (Resolve-Path -LiteralPath $candidate).Path
      }
    }
  }

  return $null
}

$buildSamples = -not $NoSamples.IsPresent
$buildTests = $WithTests.IsPresent
$enableGlfwBackend = -not $DisableGlfwBackend.IsPresent
$fetchGlfw = -not $NoFetchGlfw.IsPresent

if ($Build -and -not $buildSamples -and $Target -eq "fabrica_runtime_sample") {
  throw "Target 'fabrica_runtime_sample' requires sample build. Remove -NoSamples or change -Target"
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
  Remove-Item -LiteralPath $BuildDir -Recurse -Force
}

$expectedSourceDir = (Resolve-Path "engine").Path
$cacheFile = Join-Path $BuildDir "CMakeCache.txt"
if (Test-Path -LiteralPath $cacheFile) {
  $homeLine = Select-String -Path $cacheFile -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=' -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($homeLine -ne $null) {
    $cachedSource = $homeLine.Line.Split('=')[-1]
    if ($cachedSource -ne $expectedSourceDir) {
      Write-Host "[generate_clion] Detected build dir configured for another source ($cachedSource). Resetting $BuildDir"
      Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }
  }
}

$ninjaExe = Resolve-NinjaPath -ExplicitPath $NinjaPath
if ($ninjaExe -eq $null) {
  throw "Ninja not found. Install Ninja or CLion, or pass -NinjaPath <path-to-ninja.exe>"
}

$configureArgs = @(
  "-S", "engine",
  "-B", $BuildDir,
  "-G", "Ninja",
  "-DCMAKE_MAKE_PROGRAM=$ninjaExe",
  "-DCMAKE_BUILD_TYPE=$Config",
  "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
  "-DFABRICA_BUILD_TESTS=$(To-OnOff $buildTests)",
  "-DFABRICA_BUILD_SAMPLES=$(To-OnOff $buildSamples)",
  "-DFABRICA_ENABLE_GLFW_WINDOW_BACKEND=$(To-OnOff $enableGlfwBackend)",
  "-DFABRICA_FETCH_GLFW=$(To-OnOff $fetchGlfw)"
)

Write-Host "[generate_clion] Configure: cmake $($configureArgs -join ' ')"
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$compileDb = Join-Path $BuildDir "compile_commands.json"
if (Test-Path -LiteralPath $compileDb) {
  Copy-Item -LiteralPath $compileDb -Destination "compile_commands.json" -Force
  Copy-Item -LiteralPath $compileDb -Destination "engine/compile_commands.json" -Force
  Write-Host "[generate_clion] Copied compile_commands.json to repo root and engine/"
} else {
  Write-Host "[generate_clion] Warning: compile_commands.json not found in $BuildDir"
}

if ($Build) {
  $buildArgs = @("--build", $BuildDir)
  if ($Parallel -gt 0) {
    $buildArgs += @("--parallel", $Parallel)
  } else {
    $buildArgs += "--parallel"
  }
  if ($Target -ne "") {
    $buildArgs += @("--target", $Target)
  }

  Write-Host "[generate_clion] Build: cmake $($buildArgs -join ' ')"
  & cmake @buildArgs
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

  if ($buildTests) {
    $testArgs = @("--test-dir", $BuildDir, "--output-on-failure")
    Write-Host "[generate_clion] Test: ctest $($testArgs -join ' ')"
    & ctest @testArgs
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  }
} else {
  Write-Host "[generate_clion] Build skipped (use -Build to compile targets)"
}

Write-Host "[generate_clion] Done"
Write-Host "[generate_clion] CLion: open engine/CMakeLists.txt and use build dir '$BuildDir'"

param(
  [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
  [string]$Config = "Debug",
  [string]$BuildDir = "engine/build_vscode",
  [string]$OutputPath = "FabricaEngine.code-workspace",
  [string]$NinjaPath = "",
  [switch]$NoTests,
  [switch]$BuildSamples,
  [switch]$DisableGlfwBackend,
  [switch]$FetchGlfw
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
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
    "$env:ProgramFiles(x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe",
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

function To-WorkspacePath {
  param([string]$Path)

  $normalized = $Path.Replace('\', '/').TrimStart('./')
  if ([System.IO.Path]::IsPathRooted($Path)) {
    return $normalized
  }

  return ('${workspaceFolder}/' + $normalized)
}

$buildTests = -not $NoTests.IsPresent
$buildSamples = $BuildSamples.IsPresent
$enableGlfwBackend = -not $DisableGlfwBackend.IsPresent
$fetchGlfw = $FetchGlfw.IsPresent
$ninjaExe = Resolve-NinjaPath -ExplicitPath $NinjaPath

$configureSettings = [ordered]@{
  CMAKE_BUILD_TYPE = $Config
  CMAKE_EXPORT_COMPILE_COMMANDS = 'ON'
  FABRICA_BUILD_TESTS = To-OnOff $buildTests
  FABRICA_BUILD_SAMPLES = To-OnOff $buildSamples
  FABRICA_ENABLE_GLFW_WINDOW_BACKEND = To-OnOff $enableGlfwBackend
  FABRICA_FETCH_GLFW = To-OnOff $fetchGlfw
}

if ($ninjaExe -ne $null) {
  $configureSettings.CMAKE_MAKE_PROGRAM = $ninjaExe.Replace('\', '/')
}

$workspace = [ordered]@{
  folders = @(
    [ordered]@{
      name = 'FabricaEngine'
      path = '.'
    }
  )
  settings = [ordered]@{
    'cmake.sourceDirectory' = '${workspaceFolder}/engine'
    'cmake.buildDirectory' = To-WorkspacePath -Path $BuildDir
    'cmake.generator' = 'Ninja'
    'cmake.configureOnOpen' = $true
    'cmake.configureSettings' = $configureSettings
    'C_Cpp.default.compileCommands' = (To-WorkspacePath -Path (Join-Path $BuildDir 'compile_commands.json'))
    'clangd.arguments' = @("--compile-commands-dir=$(To-WorkspacePath -Path $BuildDir)")
  }
  extensions = [ordered]@{
    recommendations = @(
      'ms-vscode.cmake-tools',
      'llvm-vs-code-extensions.vscode-clangd',
      'ms-vscode.cpptools'
    )
  }
}

$json = $workspace | ConvertTo-Json -Depth 10
Set-Content -LiteralPath $OutputPath -Value $json -Encoding ascii

Write-Host "[generate_vscode_workspace] Wrote $OutputPath"
if ($ninjaExe -eq $null) {
  Write-Host '[generate_vscode_workspace] Warning: Ninja was not found automatically. VSCode may ask you to pick or install it.'
}
Write-Host '[generate_vscode_workspace] Open the generated .code-workspace file in VSCode to configure and index the engine.'


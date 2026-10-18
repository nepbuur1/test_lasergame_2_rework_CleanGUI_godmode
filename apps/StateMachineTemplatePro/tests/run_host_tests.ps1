param([string]$Compiler = 'g++')
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$testBuildDir = Join-Path $PSScriptRoot 'build'
New-Item -ItemType Directory -Force -Path $testBuildDir | Out-Null
$resolvedCompiler = (Get-Command $Compiler -ErrorAction Stop).Source
$compilerDir = Split-Path -Parent $resolvedCompiler
$savedPath = $env:PATH
try {
    # MSYS2's compiler and generated executable need its runtime DLLs on PATH.
    $env:PATH = "$compilerDir;$savedPath"
    $outputFile = Join-Path $testBuildDir 'host_behavior.exe'
    & $resolvedCompiler '-std=c++17' '-Wall' '-Wextra' '-Werror' '-O0' '-g' `
        '-I' (Join-Path $PSScriptRoot 'stubs') '-I' $projectDir `
        (Join-Path $PSScriptRoot 'FakeRtos.cpp') (Join-Path $PSScriptRoot 'host_behavior.cpp') `
        (Join-Path $projectDir 'app\Counter.cpp') (Join-Path $projectDir 'app\MachineControl.cpp') `
        (Join-Path $projectDir 'app\testHelpers\InputSimulator.cpp') '-o' $outputFile
    if ($LASTEXITCODE -ne 0) { throw "Host behavior test compilation failed (exit $LASTEXITCODE)." }
    & $outputFile
    if ($LASTEXITCODE -ne 0) { throw "Host behavior tests failed (exit $LASTEXITCODE)." }
} finally {
    $env:PATH = $savedPath
}

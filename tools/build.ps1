# Builds AlleviatedBackpacks for the server (BDS), the client, or both, against one LeviLamina line.
#
#   pwsh tools/build.ps1                    # both platforms, newest line (26_60), release
#   pwsh tools/build.ps1 -Target server     # one platform
#   pwsh tools/build.ps1 -Line 26_51        # another supported LeviLamina line
#   pwsh tools/build.ps1 -Tests             # also build and run the unit tests (abp-tests)
#
# Every supported line is a lip variant of the same name, with its LeviLamina version below and
# its package lock in locks/<line>.lock, which is copied over xmake-requires.lock (generated, not
# tracked). Each line and platform has its own build dir (build/<line>/<target>); outputs land in
#   bin/<target>/AlleviatedBackpacks/  (dll, pdb, manifest.json, lang/, behavior_packs/, resource_packs/)
# Copy that folder to `mods/` (BDS with LeviLamina 26.60, or the LeviLauncher client) or to
# `plugins/` (BDS with LeviLamina 26.51).
param(
    [ValidateSet('server', 'client', 'all')][string]$Target = 'all',
    [ValidateSet('26_60', '26_51')][string]$Line = '26_60',
    [ValidateSet('release', 'debug')][string]$Mode = 'release',
    [switch]$Tests
)
$ErrorActionPreference = 'Stop'
# The LeviLamina version each supported line is built against.
$leviLamina = @{ '26_60' = '26.60.6'; '26_51' = '26.51.7' }[$Line]
$root = (Resolve-Path "$PSScriptRoot/..").Path
Push-Location $root
try {
    # Fresh shells may not have the tools on PATH yet. Append (never prepend) the registry PATH so
    # the caller's PATH wins: on CI, setup-llvm puts LLVM 22 on PATH, and the runner's preinstalled
    # C:\Program Files\LLVM (listed in the machine PATH) must not shadow it.
    $env:Path = $env:Path + ';' + [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
    if ($env:LLVM_PATH -and (Test-Path (Join-Path $env:LLVM_PATH 'bin\clang-cl.exe'))) {
        $env:Path = (Join-Path $env:LLVM_PATH 'bin') + ";$env:Path"
    }
    if (-not (Get-Command clang-cl -ErrorAction SilentlyContinue)) {
        foreach ($dir in "$env:ProgramFiles\LLVM\bin", "${env:ProgramFiles(x86)}\LLVM\bin") {
            if (Test-Path (Join-Path $dir 'clang-cl.exe')) { $env:Path = "$dir;$env:Path"; break }
        }
    }
    if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) { throw 'xmake not found on PATH' }
    $clang = Get-Command clang-cl -ErrorAction SilentlyContinue
    if ($clang) { Write-Host "==> using $($clang.Source) ($((& $clang.Source --version | Select-Object -First 1)))" }
    git config --global core.longpaths true | Out-Null

    # xmake needs to be told about Visual Studio 18 (2026) explicitly.
    $vsArgs = @()
    foreach ($pf in $env:ProgramFiles, ${env:ProgramFiles(x86)}) {
        if ($pf -and (Test-Path (Join-Path $pf 'Microsoft Visual Studio\18'))) { $vsArgs = @('--vs=2026'); break }
    }

    Copy-Item (Join-Path $root "locks/$Line.lock") (Join-Path $root 'xmake-requires.lock') -Force

    $targets = if ($Target -eq 'all') { @('server', 'client') } else { @($Target) }
    foreach ($t in $targets) {
        Write-Host "==> configuring $t ($Mode, LeviLamina $leviLamina)"
        xmake f -a x64 -m $Mode -p windows --target_type=$t --levilamina=$leviLamina -o "build/$Line/$t" @vsArgs -y
        if ($LASTEXITCODE) { throw "xmake config failed for $t" }
        Write-Host "==> building $t"
        xmake build -y AlleviatedBackpacks
        if ($LASTEXITCODE) { throw "build failed for $t" }
        $out = Join-Path $root "bin/$t/AlleviatedBackpacks"
        if (-not (Test-Path (Join-Path $out 'AlleviatedBackpacks.dll'))) { throw "missing output $out" }
        Write-Host "==> $t ready: $out"
    }
    if ($Tests) {
        Write-Host '==> unit tests'
        xmake build -y abp-tests
        if ($LASTEXITCODE) { throw 'abp-tests build failed' }
        xmake run abp-tests
        if ($LASTEXITCODE) { throw 'unit tests failed' }
    }
} finally {
    Pop-Location
}

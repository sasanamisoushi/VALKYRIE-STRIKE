[CmdletBinding()]
param(
    [ValidateSet("Debug", "Development", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild,
    # デモ専用の起動ファイルも配布パッケージへ含める。
    [switch]$IncludeDemoPlay,
    # タイトルの TAB からボス撃破・クリア演出を自動再生できるようにする。
    [switch]$IncludeClearCinematicDemo
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $projectRoot "project\\CG2.sln"
$buildOutput = Join-Path $projectRoot "generated\\outputs\\$Configuration"
$packageRoot = Join-Path $projectRoot "generated\\packages"
$packageName = "ValkyrieStrike_Playable"
$packagePath = Join-Path $packageRoot $packageName
$zipPath = Join-Path $packageRoot "$packageName-$Configuration.zip"
$msbuildPath = "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"

if (-not $SkipBuild) {
    if (-not (Test-Path -LiteralPath $msbuildPath)) {
        throw "MSBuild が見つかりません: $msbuildPath"
    }

    & $msbuildPath $solutionPath /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /m
    if ($LASTEXITCODE -ne 0) {
        throw "ビルドに失敗しました。パッケージは作成していません。"
    }
}

$requiredFiles = @(
    "ValkyrieStrike.exe",
    "dxcompiler.dll",
    "dxil.dll"
)

$demoExecutable = "ValkyrieStrikeCodexDemo.exe"
if ($IncludeDemoPlay -and -not (Test-Path -LiteralPath (Join-Path $buildOutput $demoExecutable))) {
    throw "デモプレイ用ファイルが見つかりません。先にビルドしてください: $demoExecutable"
}
if (-not (Test-Path -LiteralPath (Join-Path $buildOutput "resources"))) {
    throw "resources フォルダーが見つかりません。先にビルドしてください: $buildOutput"
}

foreach ($file in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $buildOutput $file))) {
        throw "配布に必要なファイルが見つかりません: $file"
    }
}

New-Item -ItemType Directory -Force -Path $packageRoot | Out-Null
if (Test-Path -LiteralPath $packagePath) {
    Remove-Item -LiteralPath $packagePath -Recurse -Force
}
if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

New-Item -ItemType Directory -Path $packagePath | Out-Null
foreach ($file in $requiredFiles) {
    Copy-Item -LiteralPath (Join-Path $buildOutput $file) -Destination $packagePath
}
if ($IncludeDemoPlay) {
    Copy-Item -LiteralPath (Join-Path $buildOutput $demoExecutable) -Destination $packagePath
}
Copy-Item -LiteralPath (Join-Path $buildOutput "resources") -Destination $packagePath -Recurse

# どちらのデモも選ばなかった配布版では、通常EXE内のTABアトラクトデモを無効化する。
# この設定ファイルはパッケージへだけ書き込むため、開発版・通常のRelease出力には影響しない。
$runtimeOptions = @{
    allowAttractDemo = [bool]($IncludeDemoPlay -or $IncludeClearCinematicDemo)
    playClearCinematicDemo = [bool]$IncludeClearCinematicDemo
} | ConvertTo-Json
$runtimeOptions | Set-Content -LiteralPath (Join-Path $packagePath "resources\runtime_options.json") -Encoding UTF8

@"
Valkyrie Strike 実行版

ValkyrieStrike.exe をダブルクリックして起動してください。
Windows 10/11 (x64) 用です。
"@ | Set-Content -LiteralPath (Join-Path $packagePath "README.txt") -Encoding UTF8

if ($IncludeDemoPlay) {
    Add-Content -LiteralPath (Join-Path $packagePath "README.txt") -Encoding UTF8 -Value @"

デモプレイ
ValkyrieStrikeCodexDemo.exe を起動すると、デモプレイを開始します。
タイトル画面で TAB を押すと、ボス撃破から爆散、MISSION CLEAR、リザルトまでを自動再生します。
"@
}

if ($IncludeClearCinematicDemo) {
    Add-Content -LiteralPath (Join-Path $packagePath "README.txt") -Encoding UTF8 -Value @"

クリア演出デモ
タイトル画面で TAB を押すと、ボス撃破 → 爆散 → MISSION CLEAR → リザルト → タイトルを自動再生します。
"@
}

Compress-Archive -Path (Join-Path $packagePath "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "パッケージを作成しました: $packagePath"
Write-Host "ZIP を作成しました: $zipPath"
exit 0

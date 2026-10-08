[CmdletBinding()]
param(
    [ValidateSet("Debug", "Development", "Release")]
    [string]$Configuration = "Release",
    [switch]$SkipBuild
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
Copy-Item -LiteralPath (Join-Path $buildOutput "resources") -Destination $packagePath -Recurse

@"
Valkyrie Strike 実行版

ValkyrieStrike.exe をダブルクリックして起動してください。
Windows 10/11 (x64) 用です。
"@ | Set-Content -LiteralPath (Join-Path $packagePath "README.txt") -Encoding UTF8

Compress-Archive -Path (Join-Path $packagePath "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "パッケージを作成しました: $packagePath"
Write-Host "ZIP を作成しました: $zipPath"
exit 0

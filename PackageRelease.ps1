#Requires -Version 5.1

<#
.SYNOPSIS
Builds a binary release package of an Unreal Engine plugin.

.DESCRIPTION
Runs Unreal Automation Tool's BuildPlugin command, validates the packaged
plugin, removes debug symbols and build intermediates, and creates a zip
archive in the plugin's Releases folder.

BuildPlugin copies the plugin into a temporary HostProject, which repeats the
plugin path inside the package directory. Combined with a long release name
this can exceed the Windows 260 character path limit and fail the build with
"OtherCompilationError". To avoid that, the build runs in a short staging
directory (by default under %TEMP%) and the finished plugin folder is moved
into Releases\<ReleaseName> afterwards.

.PARAMETER PluginRoot
Plugin directory. Defaults to the folder containing this script, so a copy
placed in the plugin root can be run without arguments.

.PARAMETER PluginDescriptor
Path to the .uplugin file. If omitted, the script searches PluginRoot for a
single .uplugin file.

.PARAMETER EngineRoot
Path to the Unreal Engine installation. Defaults to
C:\Program Files\Epic Games\UE_<major.minor> based on EngineVersion.

.PARAMETER TargetPlatforms
Platforms passed to BuildPlugin. Defaults to Win64.

.PARAMETER ReleaseName
Release folder and archive name. Defaults to
<PluginName>-<Version>-UE<major.minor>-<Platforms>, or without the platform
suffix when -NoTargetPlatforms is used.

.PARAMETER RequireFiles
Files that must exist in the packaged plugin. Paths are relative to the
packaged plugin root.

.PARAMETER StagingRoot
Directory used for the BuildPlugin output. Defaults to %TEMP%\UEPlugin. Keep
this path short so that generated files stay below the Windows path limit.

.PARAMETER NoTargetPlatforms
Build only the host editor binaries. Useful for editor-only plugins; it is
implied automatically when every module in the descriptor is editor-only.

.PARAMETER KeepIntermediate
Keep the Intermediate directory in the packaged plugin.

.PARAMETER KeepDebugSymbols
Keep .pdb files in the packaged plugin.

.PARAMETER SkipArchive
Build and validate the package without creating a zip archive.

.EXAMPLE
.\PackageRelease.ps1 -PluginRoot "C:\Projects\MyProject\Plugins\MyPlugin"

.EXAMPLE
.\PackageRelease.ps1 `
    -PluginRoot "C:\Projects\MyProject\Plugins\MyPlugin" `
    -TargetPlatforms Win64 `
    -RequireFiles "Content\..."
#>

[CmdletBinding()]
param(
    [string]$PluginRoot,
    [string]$PluginDescriptor,
    [string]$EngineRoot,
    [string[]]$TargetPlatforms = @("Win64"),
    [string]$ReleaseName,
    [string[]]$RequireFiles = @(),
    [string]$StagingRoot,
    [switch]$NoTargetPlatforms,
    [switch]$KeepIntermediate,
    [switch]$KeepDebugSymbols,
    [switch]$SkipArchive
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $PluginRoot) {
    $PluginRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
}

$PluginRoot = (Resolve-Path -LiteralPath $PluginRoot).Path

if (-not (Test-Path -LiteralPath $PluginRoot -PathType Container)) {
    throw "Plugin root is not a directory: $PluginRoot"
}

if (-not $PluginDescriptor) {
    $Descriptors = @(Get-ChildItem -Path $PluginRoot -Filter "*.uplugin" -File)
    if ($Descriptors.Count -eq 0) {
        throw "No .uplugin file found in $PluginRoot"
    }
    if ($Descriptors.Count -gt 1) {
        throw "Multiple .uplugin files found in $PluginRoot. Use -PluginDescriptor to choose one."
    }
    $PluginDescriptor = $Descriptors[0].FullName
}

$PluginDescriptor = (Resolve-Path -LiteralPath $PluginDescriptor).Path
$PluginRoot = Split-Path -Parent $PluginDescriptor
$PluginFolderName = Split-Path -Leaf $PluginRoot
$PluginName = [System.IO.Path]::GetFileNameWithoutExtension($PluginDescriptor)

$PluginInfo = Get-Content -LiteralPath $PluginDescriptor -Raw | ConvertFrom-Json

# Build host editor binaries only when every module is editor-only, so that
# editor plugins can be packaged without passing -NoTargetPlatforms.
$ModuleTypes = @()
if ($PluginInfo.PSObject.Properties["Modules"]) {
    $ModuleTypes = @($PluginInfo.Modules | ForEach-Object { [string]$_.Type })
}

$EditorOnlyModuleTypes = @("Editor", "EditorNoCommandlet", "UncookedOnly")
$RuntimeModules = @($ModuleTypes | Where-Object { $EditorOnlyModuleTypes -notcontains $_ })
$IsEditorOnlyPlugin = ($ModuleTypes.Count -gt 0) -and ($RuntimeModules.Count -eq 0)

if (-not $NoTargetPlatforms -and $IsEditorOnlyPlugin -and -not $PSBoundParameters.ContainsKey("TargetPlatforms")) {
    $NoTargetPlatforms = $true
    Write-Host "Editor-only plugin: building host editor binaries only."
}

$VersionName = if ($PluginInfo.PSObject.Properties["VersionName"]) {
    [string]$PluginInfo.VersionName
} else {
    "1.0"
}

$EngineVersion = if ($PluginInfo.PSObject.Properties["EngineVersion"]) {
    [string]$PluginInfo.EngineVersion
} else {
    "5.8.0"
}

$EngineVersionParts = $EngineVersion -split '\.'
$EngineVersionShort = if ($EngineVersionParts.Count -ge 2) {
    "$($EngineVersionParts[0]).$($EngineVersionParts[1])"
} else {
    $EngineVersion
}

if (-not $EngineRoot) {
    $EngineRoot = "C:\Program Files\Epic Games\UE_$EngineVersionShort"
}

$RunUAT = Join-Path $EngineRoot "Engine\Build\BatchFiles\RunUAT.bat"
if (-not (Test-Path -LiteralPath $RunUAT)) {
    throw "RunUAT.bat not found: $RunUAT. Use -EngineRoot to specify the Unreal Engine installation."
}

if (-not $NoTargetPlatforms) {
    $TargetPlatforms = @($TargetPlatforms | Where-Object { $_ })
    if ($TargetPlatforms.Count -eq 0) {
        throw "No target platforms specified. Use -TargetPlatforms or -NoTargetPlatforms."
    }
}

$PlatformSuffix = if ($NoTargetPlatforms) {
    ""
} else {
    "-" + ($TargetPlatforms -join "-")
}

if (-not $ReleaseName) {
    $ReleaseName = "$PluginName-$VersionName-UE$EngineVersionShort$PlatformSuffix"
}

$ReleasesDir = Join-Path $PluginRoot "Releases"
$PackageDir = Join-Path $ReleasesDir $ReleaseName
$ArchivePath = Join-Path $ReleasesDir "$ReleaseName.zip"

if (-not $StagingRoot) {
    $StagingRoot = Join-Path ([System.IO.Path]::GetTempPath()) "UEPlugin"
}
$StagingRoot = [System.IO.Path]::GetFullPath($StagingRoot)
$StagingDir = Join-Path $StagingRoot ".build"

if (Test-Path -LiteralPath $StagingDir) {
    Remove-Item -LiteralPath $StagingDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $StagingDir | Out-Null

New-Item -ItemType Directory -Force -Path $ReleasesDir | Out-Null

if (Test-Path -LiteralPath $PackageDir) {
    Remove-Item -LiteralPath $PackageDir -Recurse -Force
}

if (Test-Path -LiteralPath $ArchivePath) {
    Remove-Item -LiteralPath $ArchivePath -Force
}

Write-Host "Building $PluginName $VersionName for UE $EngineVersionShort..."
Write-Host "Staging directory: $StagingDir"
Write-Host "Package directory: $PackageDir"

$BuildArgs = @(
    "BuildPlugin",
    "-Plugin=$PluginDescriptor",
    "-Package=$StagingDir",
    "-PackageAppendPluginSubdir"
)

if ($NoTargetPlatforms) {
    $BuildArgs += "-NoTargetPlatforms"
} else {
    $BuildArgs += "-TargetPlatforms=$($TargetPlatforms -join '+')"
}

& $RunUAT @BuildArgs

if ($LASTEXITCODE -ne 0) {
    throw "BuildPlugin failed with exit code $LASTEXITCODE. Staging output kept in $StagingDir."
}

$StagedPluginDir = Join-Path $StagingDir $PluginFolderName
if (-not (Test-Path -LiteralPath $StagedPluginDir)) {
    throw "Packaged plugin directory not found: $StagedPluginDir"
}

New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Move-Item -LiteralPath $StagedPluginDir -Destination $PackageDir

Remove-Item -LiteralPath $StagingDir -Recurse -Force -ErrorAction SilentlyContinue

$PackagedPluginDir = Join-Path $PackageDir $PluginFolderName
$BinariesDir = Join-Path $PackagedPluginDir "Binaries"

if (-not (Test-Path -LiteralPath $PackagedPluginDir)) {
    throw "Packaged plugin directory not found: $PackagedPluginDir"
}

$ModulesFiles = @(
    Get-ChildItem -Path $BinariesDir -Recurse -Filter "UnrealEditor.modules" -File -ErrorAction SilentlyContinue
)
if ($ModulesFiles.Count -eq 0) {
    throw "UnrealEditor.modules not found under $BinariesDir"
}

$RuntimeDlls = @(
    Get-ChildItem -Path $BinariesDir -Recurse -Filter "UnrealEditor-$PluginName*.dll" -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch "DebugGame" }
)
if ($RuntimeDlls.Count -eq 0) {
    throw "No release DLL found under $BinariesDir. Expected UnrealEditor-$PluginName.dll."
}

foreach ($RequiredFile in $RequireFiles) {
    $RequiredPath = Join-Path $PackagedPluginDir $RequiredFile
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Required file or directory not found in the package: $RequiredFile. Check Config\FilterPlugin.ini."
    }
}

if (-not $KeepDebugSymbols) {
    Get-ChildItem -Path $PackagedPluginDir -Recurse -Filter "*.pdb" -File |
        Remove-Item -Force
}

if (-not $KeepIntermediate) {
    $IntermediateDir = Join-Path $PackagedPluginDir "Intermediate"
    if (Test-Path -LiteralPath $IntermediateDir) {
        Remove-Item -LiteralPath $IntermediateDir -Recurse -Force
    }
}

Write-Host "Release package: $PackagedPluginDir"

if (-not $SkipArchive) {
    Write-Host "Creating archive: $ArchivePath"
    try {
        Compress-Archive -Path $PackagedPluginDir -DestinationPath $ArchivePath -CompressionLevel Optimal -ErrorAction Stop
    } catch {
        Write-Host "Compress-Archive unavailable, falling back to .NET ZipFile."
        if (Test-Path -LiteralPath $ArchivePath) {
            Remove-Item -LiteralPath $ArchivePath -Force
        }
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        [System.IO.Compression.ZipFile]::CreateFromDirectory(
            $PackageDir,
            $ArchivePath,
            [System.IO.Compression.CompressionLevel]::Optimal,
            $false)
    }

    $ArchiveSizeMb = [math]::Round((Get-Item -LiteralPath $ArchivePath).Length / 1MB, 2)
    Write-Host "Release archive created: $ArchivePath ($ArchiveSizeMb MB)"
}

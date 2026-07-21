param(
    [string[]]$TargetPath = @(),
    [string]$TerminalRoot = "",
    [ValidateSet("Auto", "MQL4", "MQL5", "Both")]
    [string]$Platform = "Auto",
    [switch]$DryRun,
    [switch]$RequireTarget
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Get-DefaultTerminalRoot {
    if ([string]::IsNullOrWhiteSpace($env:APPDATA)) {
        return ""
    }
    return Join-Path $env:APPDATA "MetaQuotes\Terminal"
}

function Get-RequestedPlatforms {
    param([string]$Value)

    switch ($Value) {
        "MQL4" { return @("MQL4") }
        "MQL5" { return @("MQL5") }
        "Both" { return @("MQL4", "MQL5") }
        default { return @("MQL4", "MQL5") }
    }
}

function Test-RequestedPlatform {
    param(
        [string]$Runtime,
        [string]$Selection
    )

    return (Get-RequestedPlatforms $Selection) -contains $Runtime
}

function New-InstallTarget {
    param(
        [string]$Runtime,
        [string]$MqlRoot
    )

    return [pscustomobject]@{
        Runtime = $Runtime
        MqlRoot = (Resolve-Path -LiteralPath $MqlRoot).Path
    }
}

function Add-TerminalDataTargets {
    param(
        [string]$DataRoot,
        [string]$Selection,
        [System.Collections.Generic.List[object]]$Targets
    )

    foreach ($runtime in @("MQL4", "MQL5")) {
        $mqlRoot = Join-Path $DataRoot $runtime
        if ((Test-RequestedPlatform $runtime $Selection) -and
            (Test-Path -LiteralPath $mqlRoot -PathType Container)) {
            $Targets.Add((New-InstallTarget -Runtime $runtime -MqlRoot $mqlRoot))
        }
    }
}

function Add-ExplicitTarget {
    param(
        [string]$Path,
        [string]$Selection,
        [System.Collections.Generic.List[object]]$Targets
    )

    if (!(Test-Path -LiteralPath $Path -PathType Container)) {
        throw "MetaTrader target path does not exist or is not a directory: $Path"
    }

    $resolved = (Resolve-Path -LiteralPath $Path).Path
    $leaf = Split-Path -Leaf $resolved
    if ($leaf -eq "MQL4" -or $leaf -eq "MQL5") {
        if (!(Test-RequestedPlatform $leaf $Selection)) {
            return
        }
        $Targets.Add((New-InstallTarget -Runtime $leaf -MqlRoot $resolved))
        return
    }

    $before = $Targets.Count
    Add-TerminalDataTargets -DataRoot $resolved -Selection $Selection -Targets $Targets
    if ($Targets.Count -eq $before) {
        throw "MetaTrader target path must be a terminal data folder or MQL4/MQL5 root: $Path"
    }
}

function Discover-Targets {
    param(
        [string[]]$ExplicitTargets,
        [string]$Root,
        [string]$Selection
    )

    $targets = [System.Collections.Generic.List[object]]::new()
    if ($ExplicitTargets.Count -gt 0) {
        foreach ($path in $ExplicitTargets) {
            Add-ExplicitTarget -Path $path -Selection $Selection -Targets $targets
        }
        return $targets
    }

    if ([string]::IsNullOrWhiteSpace($Root)) {
        $Root = Get-DefaultTerminalRoot
    }
    if ([string]::IsNullOrWhiteSpace($Root) -or
        !(Test-Path -LiteralPath $Root -PathType Container)) {
        return $targets
    }

    foreach ($child in Get-ChildItem -LiteralPath $Root -Directory) {
        Add-TerminalDataTargets `
            -DataRoot $child.FullName `
            -Selection $Selection `
            -Targets $targets
    }
    return $targets
}

function Copy-OptionXSourceFiles {
    param(
        [string]$Runtime,
        [string]$Source,
        [string]$Destination,
        [switch]$DryRun
    )

    if ($DryRun) {
        Write-Host "[dry-run] $Runtime copy source files $Source -> $Destination"
        return
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null

    $sourceRoot = (Resolve-Path -LiteralPath $Source).Path.TrimEnd([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
    $allowedExtensions = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    [void]$allowedExtensions.Add(".mqh")
    [void]$allowedExtensions.Add(".mq4")
    [void]$allowedExtensions.Add(".mq5")

    foreach ($file in Get-ChildItem -LiteralPath $Source -Recurse -File) {
        if (!$allowedExtensions.Contains($file.Extension)) {
            continue
        }

        $relativePath = $file.FullName.Substring($sourceRoot.Length).TrimStart([System.IO.Path]::DirectorySeparatorChar, [System.IO.Path]::AltDirectorySeparatorChar)
        $destinationPath = Join-Path $Destination $relativePath
        New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destinationPath) | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destinationPath -Force
    }
}

function Copy-OptionXTree {
    param(
        [string]$Runtime,
        [string]$MqlRoot,
        [switch]$DryRun
    )

    $repoRoot = Get-RepoRoot
    $sourceRoot = Join-Path $repoRoot (Join-Path "mql" $Runtime)
    $sourceInclude = Join-Path $sourceRoot "Include\OptionX"
    $sourceIndicators = Join-Path $sourceRoot "Indicators\OptionX"
    if (!(Test-Path -LiteralPath $sourceInclude -PathType Container)) {
        throw "OptionX $Runtime include source not found: $sourceInclude"
    }
    if (!(Test-Path -LiteralPath $sourceIndicators -PathType Container)) {
        throw "OptionX $Runtime indicator source not found: $sourceIndicators"
    }

    $destinationInclude = Join-Path $MqlRoot "Include\OptionX"
    $destinationIndicators = Join-Path $MqlRoot "Indicators\OptionX"

    foreach ($copy in @(
        [pscustomobject]@{ Source = $sourceInclude; Destination = $destinationInclude },
        [pscustomobject]@{ Source = $sourceIndicators; Destination = $destinationIndicators }
    )) {
        if ($DryRun) {
            Copy-OptionXSourceFiles `
                -Runtime $Runtime `
                -Source $copy.Source `
                -Destination $copy.Destination `
                -DryRun
            continue
        }
        Copy-OptionXSourceFiles `
            -Runtime $Runtime `
            -Source $copy.Source `
            -Destination $copy.Destination
        Write-Host "$Runtime installed: $($copy.Destination)"
    }
}

$targets = @(Discover-Targets `
    -ExplicitTargets $TargetPath `
    -Root $TerminalRoot `
    -Selection $Platform)

if ($targets.Count -eq 0) {
    $message = "No matching MetaTrader MQL targets found."
    if ($RequireTarget) {
        throw $message
    }
    Write-Host $message
    exit 0
}

$seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
$installed = 0
foreach ($target in $targets) {
    $key = "$($target.Runtime)|$($target.MqlRoot)"
    if (!$seen.Add($key)) {
        continue
    }
    Copy-OptionXTree `
        -Runtime $target.Runtime `
        -MqlRoot $target.MqlRoot `
        -DryRun:$DryRun
    ++$installed
}

Write-Host "OptionX MetaTrader MQL install complete: targets=$installed dry_run=$([bool]$DryRun)"

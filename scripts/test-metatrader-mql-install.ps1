param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build/metatrader-mql-install-self-test"
}
$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName

function New-FakeMqlRuntime {
    param(
        [string]$TerminalRoot,
        [string]$Runtime
    )

    New-Item -ItemType Directory -Force -Path (Join-Path $TerminalRoot "$Runtime\Include") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $TerminalRoot "$Runtime\Indicators") | Out-Null
}

function Assert-Installed {
    param(
        [string]$MqlRoot,
        [string]$Runtime
    )

    $header = Join-Path $MqlRoot "Include\OptionX\OptionXFileBridge.mqh"
    $extension = if ($Runtime -eq "MQL4") { "mq4" } else { "mq5" }
    $indicator = Join-Path $MqlRoot "Indicators\OptionX\OptionXFileBridgeSignalExample.$extension"
    if (!(Test-Path -LiteralPath $header -PathType Leaf)) {
        throw "Expected installed header: $header"
    }
    if (!(Test-Path -LiteralPath $indicator -PathType Leaf)) {
        throw "Expected installed indicator: $indicator"
    }
}

function Assert-NotInstalled {
    param([string]$MqlRoot)

    $header = Join-Path $MqlRoot "Include\OptionX\OptionXFileBridge.mqh"
    if (Test-Path -LiteralPath $header) {
        throw "Dry-run unexpectedly installed: $header"
    }
}

$explicitTerminal = Join-Path $OutputDir "portable-terminal"
New-FakeMqlRuntime -TerminalRoot $explicitTerminal -Runtime "MQL5"

& (Join-Path $PSScriptRoot "install-metatrader-mql.ps1") `
    -TargetPath $explicitTerminal `
    -DryRun `
    -RequireTarget
Assert-NotInstalled -MqlRoot (Join-Path $explicitTerminal "MQL5")

& (Join-Path $PSScriptRoot "install-metatrader-mql.ps1") `
    -TargetPath $explicitTerminal `
    -RequireTarget
Assert-Installed -MqlRoot (Join-Path $explicitTerminal "MQL5") -Runtime "MQL5"

$explicitMqlRoot = Join-Path $OutputDir "direct-mql4\MQL4"
New-FakeMqlRuntime -TerminalRoot (Split-Path -Parent $explicitMqlRoot) -Runtime "MQL4"
& (Join-Path $PSScriptRoot "install-metatrader-mql.ps1") `
    -TargetPath $explicitMqlRoot `
    -Platform MQL4 `
    -RequireTarget
Assert-Installed -MqlRoot $explicitMqlRoot -Runtime "MQL4"

$terminalRoot = Join-Path $OutputDir "MetaQuotes\Terminal"
$mt4 = Join-Path $terminalRoot "mt4-terminal"
$mt5 = Join-Path $terminalRoot "mt5-terminal"
New-FakeMqlRuntime -TerminalRoot $mt4 -Runtime "MQL4"
New-FakeMqlRuntime -TerminalRoot $mt5 -Runtime "MQL5"
New-Item -ItemType Directory -Force -Path (Join-Path $terminalRoot "Common\Files") | Out-Null

& (Join-Path $PSScriptRoot "install-metatrader-mql.ps1") `
    -TerminalRoot $terminalRoot `
    -RequireTarget
Assert-Installed -MqlRoot (Join-Path $mt4 "MQL4") -Runtime "MQL4"
Assert-Installed -MqlRoot (Join-Path $mt5 "MQL5") -Runtime "MQL5"

try {
    & (Join-Path $PSScriptRoot "install-metatrader-mql.ps1") `
        -TargetPath (Join-Path $OutputDir "missing") `
        -RequireTarget
    throw "Expected missing explicit target to fail."
} catch {
    if ($_.Exception.Message -notlike "*does not exist*") {
        throw
    }
    Write-Host "missing explicit target failed as expected."
}

Write-Host "MetaTrader MQL install script self-test passed."

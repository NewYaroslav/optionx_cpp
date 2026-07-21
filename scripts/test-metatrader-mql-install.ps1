param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build/metatrader-mql-install-self-test"
}
$OutputDir = Join-Path (New-Item -ItemType Directory -Force -Path $OutputDir).FullName ([Guid]::NewGuid().ToString("N"))
$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName
$installer = Join-Path $PSScriptRoot "install-metatrader-mql.ps1"

function New-FakeMqlRuntime {
    param(
        [string]$TerminalRoot,
        [string]$Runtime
    )

    New-Item -ItemType Directory -Force -Path (Join-Path $TerminalRoot "$Runtime\Include") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $TerminalRoot "$Runtime\Indicators") | Out-Null
}

function Assert-PathExists {
    param([string]$Path)

    if (!(Test-Path -LiteralPath $Path)) {
        throw "Expected path to exist: $Path"
    }
}

function Assert-PathMissing {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        throw "Expected path to be absent: $Path"
    }
}

function Assert-Installed {
    param(
        [string]$MqlRoot,
        [string]$Runtime
    )

    $header = Join-Path $MqlRoot "Include\OptionX\OptionXFileBridge.mqh"
    $extension = if ($Runtime -eq "MQL4") { "mq4" } else { "mq5" }
    $indicator = Join-Path $MqlRoot "Indicators\OptionX\OptionXFileBridgeSignalExample.$extension"
    Assert-PathExists -Path $header
    Assert-PathExists -Path $indicator
}

function Assert-NotInstalled {
    param([string]$MqlRoot)

    Assert-PathMissing -Path (Join-Path $MqlRoot "Include\OptionX\OptionXFileBridge.mqh")
}

function Assert-FileMatches {
    param(
        [string]$Expected,
        [string]$Actual
    )

    Assert-PathExists -Path $Actual
    $expectedHash = (Get-FileHash -LiteralPath $Expected -Algorithm SHA256).Hash
    $actualHash = (Get-FileHash -LiteralPath $Actual -Algorithm SHA256).Hash
    if ($expectedHash -ne $actualHash) {
        throw "File content mismatch: $Actual"
    }
}

function Add-SelfTestSourceFiles {
    $includeDir = Join-Path $repoRoot "mql\MQL5\Include\OptionX\SelfTestNested"
    $indicatorDir = Join-Path $repoRoot "mql\MQL5\Indicators\OptionX\SelfTestNested"
    New-Item -ItemType Directory -Force -Path $includeDir | Out-Null
    New-Item -ItemType Directory -Force -Path $indicatorDir | Out-Null

    Set-Content -LiteralPath (Join-Path $includeDir "Allowed.mqh") -NoNewline -Encoding ASCII -Value "// installer self-test header"
    Set-Content -LiteralPath (Join-Path $includeDir "Compiled.ex5") -NoNewline -Encoding ASCII -Value "compiled"
    Set-Content -LiteralPath (Join-Path $includeDir "install.log") -NoNewline -Encoding ASCII -Value "log"
    Set-Content -LiteralPath (Join-Path $includeDir "temp.tmp") -NoNewline -Encoding ASCII -Value "tmp"
    Set-Content -LiteralPath (Join-Path $includeDir "junk.txt") -NoNewline -Encoding ASCII -Value "junk"
    Set-Content -LiteralPath (Join-Path $indicatorDir "AllowedIndicator.mq5") -NoNewline -Encoding ASCII -Value "// installer self-test indicator"
}

function Remove-SelfTestSourceFiles {
    foreach ($path in @(
        (Join-Path $repoRoot "mql\MQL5\Include\OptionX\SelfTestNested"),
        (Join-Path $repoRoot "mql\MQL5\Indicators\OptionX\SelfTestNested")
    )) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

Remove-SelfTestSourceFiles
Add-SelfTestSourceFiles

try {
    $explicitTerminal = Join-Path $OutputDir "portable-terminal"
    New-FakeMqlRuntime -TerminalRoot $explicitTerminal -Runtime "MQL5"

    & $installer `
        -TargetPath $explicitTerminal `
        -DryRun `
        -RequireTarget
    Assert-NotInstalled -MqlRoot (Join-Path $explicitTerminal "MQL5")

    & $installer `
        -TargetPath $explicitTerminal `
        -RequireTarget
    Assert-Installed -MqlRoot (Join-Path $explicitTerminal "MQL5") -Runtime "MQL5"

    $explicitMqlRoot = Join-Path $OutputDir "direct-mql4\MQL4"
    New-FakeMqlRuntime -TerminalRoot (Split-Path -Parent $explicitMqlRoot) -Runtime "MQL4"
    & $installer `
        -TargetPath $explicitMqlRoot `
        -Platform MQL4 `
        -RequireTarget
    Assert-Installed -MqlRoot $explicitMqlRoot -Runtime "MQL4"

    $emptyMql5Root = Join-Path $OutputDir "direct-empty\MQL5"
    New-Item -ItemType Directory -Force -Path $emptyMql5Root | Out-Null
    & $installer `
        -TargetPath $emptyMql5Root `
        -Platform MQL5 `
        -DryRun `
        -RequireTarget
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Include")
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Indicators")

    & $installer `
        -TargetPath $emptyMql5Root `
        -Platform MQL5 `
        -RequireTarget
    Assert-Installed -MqlRoot $emptyMql5Root -Runtime "MQL5"
    Assert-PathExists -Path (Join-Path $emptyMql5Root "Include\OptionX\SelfTestNested\Allowed.mqh")
    Assert-PathExists -Path (Join-Path $emptyMql5Root "Indicators\OptionX\SelfTestNested\AllowedIndicator.mq5")
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Include\OptionX\SelfTestNested\Compiled.ex5")
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Include\OptionX\SelfTestNested\install.log")
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Include\OptionX\SelfTestNested\temp.tmp")
    Assert-PathMissing -Path (Join-Path $emptyMql5Root "Include\OptionX\SelfTestNested\junk.txt")

    $sourceHeader = Join-Path $repoRoot "mql\MQL5\Include\OptionX\OptionXFileBridge.mqh"
    $destinationHeader = Join-Path $emptyMql5Root "Include\OptionX\OptionXFileBridge.mqh"
    Set-Content -LiteralPath $destinationHeader -NoNewline -Encoding ASCII -Value "corrupted"
    & $installer `
        -TargetPath $emptyMql5Root `
        -Platform MQL5 `
        -RequireTarget
    Assert-FileMatches -Expected $sourceHeader -Actual $destinationHeader

    $staleFile = Join-Path $emptyMql5Root "Include\OptionX\user-local.txt"
    Set-Content -LiteralPath $staleFile -NoNewline -Encoding ASCII -Value "keep me"
    & $installer `
        -TargetPath $emptyMql5Root `
        -Platform MQL5 `
        -RequireTarget
    Assert-PathExists -Path $staleFile
    if ((Get-Content -LiteralPath $staleFile -Raw) -ne "keep me") {
        throw "Installer unexpectedly changed stale destination file: $staleFile"
    }

    $bothTerminal = Join-Path $OutputDir "both-terminal"
    New-FakeMqlRuntime -TerminalRoot $bothTerminal -Runtime "MQL4"
    New-FakeMqlRuntime -TerminalRoot $bothTerminal -Runtime "MQL5"
    & $installer `
        -TargetPath $bothTerminal `
        -Platform MQL5 `
        -RequireTarget
    Assert-NotInstalled -MqlRoot (Join-Path $bothTerminal "MQL4")
    Assert-Installed -MqlRoot (Join-Path $bothTerminal "MQL5") -Runtime "MQL5"

    $multiTerminalA = Join-Path $OutputDir "multi-terminal-a"
    $multiTerminalB = Join-Path $OutputDir "multi-terminal-b"
    New-FakeMqlRuntime -TerminalRoot $multiTerminalA -Runtime "MQL5"
    New-FakeMqlRuntime -TerminalRoot $multiTerminalB -Runtime "MQL5"
    & $installer `
        -TargetPath $multiTerminalA, $multiTerminalB `
        -Platform MQL5 `
        -RequireTarget
    Assert-Installed -MqlRoot (Join-Path $multiTerminalA "MQL5") -Runtime "MQL5"
    Assert-Installed -MqlRoot (Join-Path $multiTerminalB "MQL5") -Runtime "MQL5"

    $dedupTerminal = Join-Path $OutputDir "dedup-terminal"
    New-FakeMqlRuntime -TerminalRoot $dedupTerminal -Runtime "MQL5"
    $dedupOutput = & $installer `
        -TargetPath $dedupTerminal, (Join-Path $dedupTerminal "MQL5") `
        -Platform MQL5 `
        -RequireTarget 6>&1 | Out-String
    if ($dedupOutput -notmatch "targets=1") {
        throw "Expected deduplicated target count in installer output. Output: $dedupOutput"
    }
    Assert-Installed -MqlRoot (Join-Path $dedupTerminal "MQL5") -Runtime "MQL5"

    $terminalRoot = Join-Path $OutputDir "MetaQuotes\Terminal"
    $mt4 = Join-Path $terminalRoot "mt4-terminal"
    $mt5 = Join-Path $terminalRoot "mt5-terminal"
    New-FakeMqlRuntime -TerminalRoot $mt4 -Runtime "MQL4"
    New-FakeMqlRuntime -TerminalRoot $mt5 -Runtime "MQL5"
    New-Item -ItemType Directory -Force -Path (Join-Path $terminalRoot "Common\Files") | Out-Null

    & $installer `
        -TerminalRoot $terminalRoot `
        -RequireTarget
    Assert-Installed -MqlRoot (Join-Path $mt4 "MQL4") -Runtime "MQL4"
    Assert-Installed -MqlRoot (Join-Path $mt5 "MQL5") -Runtime "MQL5"

    try {
        & $installer `
            -TargetPath (Join-Path $OutputDir "missing") `
            -RequireTarget
        throw "Expected missing explicit target to fail."
    } catch {
        if ($_.Exception.Message -notlike "*does not exist*") {
            throw
        }
        Write-Host "missing explicit target failed as expected."
    }

    $oldAppData = $env:APPDATA
    try {
        $env:APPDATA = Join-Path $OutputDir "empty-appdata"
        New-Item -ItemType Directory -Force -Path $env:APPDATA | Out-Null
        try {
            & $installer -RequireTarget
            throw "Expected empty default discovery to fail."
        } catch {
            if ($_.Exception.Message -notlike "*No matching MetaTrader MQL targets found*") {
                throw
            }
            Write-Host "empty default discovery failed as expected."
        }
    } finally {
        if ($null -eq $oldAppData) {
            Remove-Item Env:APPDATA -ErrorAction SilentlyContinue
        } else {
            $env:APPDATA = $oldAppData
        }
    }
} finally {
    Remove-SelfTestSourceFiles
}

Write-Host "MetaTrader MQL install script self-test passed."

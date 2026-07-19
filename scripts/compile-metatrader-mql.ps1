param(
    [string]$MetaEditor5Path = $env:OPTIONX_METAEDITOR5_PATH,
    [string]$MetaEditor4Path = $env:OPTIONX_METAEDITOR4_PATH,
    [string]$OutputDir = "",
    [int]$TimeoutSeconds = 60,
    [switch]$RequireMql5,
    [switch]$RequireMql4,
    [switch]$KeepArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-RepoRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

function Join-RepoPath {
    param([string]$RelativePath)
    return Join-Path (Get-RepoRoot) $RelativePath
}

function Resolve-ExistingFile {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return ""
    }
    $resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if ($null -eq $resolved) {
        return ""
    }
    return $resolved.Path
}

function Find-MetaEditor {
    param(
        [string]$ExplicitPath,
        [string[]]$CandidateRoots
    )

    if (![string]::IsNullOrWhiteSpace($ExplicitPath)) {
        $explicit = Resolve-ExistingFile $ExplicitPath
        if (!$explicit) {
            throw "Explicit MetaEditor path does not exist: $ExplicitPath"
        }
        return $explicit
    }

    foreach ($root in $CandidateRoots) {
        if ([string]::IsNullOrWhiteSpace($root)) {
            continue
        }

        foreach ($name in @("MetaEditor64.exe", "metaeditor64.exe", "MetaEditor.exe", "metaeditor.exe")) {
            $candidate = Join-Path $root $name
            $resolved = Resolve-ExistingFile $candidate
            if ($resolved) {
                return $resolved
            }
        }
    }

    return ""
}

function Wait-LogFile {
    param([string]$Path)

    for ($attempt = 0; $attempt -lt 50; ++$attempt) {
        if ((Test-Path -LiteralPath $Path) -and
            ((Get-Item -LiteralPath $Path).Length -gt 0)) {
            return
        }
        Start-Sleep -Milliseconds 200
    }
}

function Read-CompileResult {
    param([string]$LogPath)

    if (!(Test-Path -LiteralPath $LogPath)) {
        throw "MetaEditor did not write a compile log: $LogPath"
    }

    $text = Get-Content -Raw -LiteralPath $LogPath
    $match = [regex]::Match(
        $text,
        "Result:\s*(\d+)\s+errors?,\s*(\d+)\s+warnings?",
        [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    if (!$match.Success) {
        throw "MetaEditor compile log does not contain a Result line: $LogPath"
    }

    return [pscustomobject]@{
        Errors = [int]$match.Groups[1].Value
        Warnings = [int]$match.Groups[2].Value
        Text = $text
    }
}

function Invoke-MqlCompile {
    param(
        [string]$Name,
        [string]$MetaEditorPath,
        [string]$SourcePath,
        [string]$IncludeRoot,
        [string]$LogPath,
        [string]$ArtifactPath,
        [switch]$Required
    )

    if ([string]::IsNullOrWhiteSpace($MetaEditorPath)) {
        $message = "$Name MetaEditor not found; skipping $Name compile smoke."
        if ($Required) {
            throw $message
        }
        Write-Host $message
        return [pscustomobject]@{
            Name = $Name
            Status = "skipped"
            Errors = 0
            Warnings = 0
            Log = $LogPath
        }
    }

    if (!(Test-Path -LiteralPath $SourcePath)) {
        throw "$Name source file not found: $SourcePath"
    }
    if (!(Test-Path -LiteralPath $IncludeRoot)) {
        throw "$Name include root not found: $IncludeRoot"
    }

    Remove-Item -LiteralPath $LogPath -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $ArtifactPath -ErrorAction SilentlyContinue

    Write-Host "Compiling $Name with $MetaEditorPath"
    $arguments = @(
        "/compile:`"$SourcePath`"",
        "/inc:`"$IncludeRoot`"",
        "/log:`"$LogPath`""
    )
    $process = Start-Process `
        -FilePath $MetaEditorPath `
        -ArgumentList $arguments `
        -WindowStyle Hidden `
        -PassThru
    if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        $process.WaitForExit(5000) | Out-Null
        throw "$Name MetaEditor compile timed out after $TimeoutSeconds seconds."
    }
    Wait-LogFile $LogPath

    $result = Read-CompileResult $LogPath
    if ($result.Errors -ne 0) {
        Write-Host $result.Text
        throw "$Name compile failed with $($result.Errors) errors and $($result.Warnings) warnings."
    }
    if (!(Test-Path -LiteralPath $ArtifactPath -PathType Leaf)) {
        Write-Host $result.Text
        throw "$Name reported zero errors but did not produce: $ArtifactPath"
    }

    if (!$KeepArtifacts) {
        Remove-Item -LiteralPath $ArtifactPath -ErrorAction SilentlyContinue
    }

    Write-Host "$Name compile passed with $($result.Warnings) warnings."
    return [pscustomobject]@{
        Name = $Name
        Status = "passed"
        Errors = $result.Errors
        Warnings = $result.Warnings
        ExitCode = $process.ExitCode
        Log = $LogPath
    }
}

$repoRoot = Get-RepoRoot
if ($TimeoutSeconds -le 0) {
    throw "TimeoutSeconds must be greater than zero."
}

if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build/metaeditor-smoke"
}
$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName

$programFiles = @(
    ${env:ProgramFiles},
    ${env:ProgramFiles(x86)}
) | Where-Object { ![string]::IsNullOrWhiteSpace($_) }

$mt5Editor = Find-MetaEditor `
    -ExplicitPath $MetaEditor5Path `
    -CandidateRoots ($programFiles | ForEach-Object { Join-Path $_ "MetaTrader 5" })
$mt4Editor = Find-MetaEditor `
    -ExplicitPath $MetaEditor4Path `
    -CandidateRoots ($programFiles | ForEach-Object { Join-Path $_ "MetaTrader 4" })

$results = @()
$results += Invoke-MqlCompile `
    -Name "MQL5" `
    -MetaEditorPath $mt5Editor `
    -SourcePath (Join-RepoPath "mql/MQL5/Indicators/OptionX/OptionXFileBridgeSignalExample.mq5") `
    -IncludeRoot (Join-RepoPath "mql/MQL5") `
    -LogPath (Join-Path $OutputDir "mql5-optionx-file-bridge.log") `
    -ArtifactPath (Join-RepoPath "mql/MQL5/Indicators/OptionX/OptionXFileBridgeSignalExample.ex5") `
    -Required:$RequireMql5

$results += Invoke-MqlCompile `
    -Name "MQL4" `
    -MetaEditorPath $mt4Editor `
    -SourcePath (Join-RepoPath "mql/MQL4/Indicators/OptionX/OptionXFileBridgeSignalExample.mq4") `
    -IncludeRoot (Join-RepoPath "mql/MQL4") `
    -LogPath (Join-Path $OutputDir "mql4-optionx-file-bridge.log") `
    -ArtifactPath (Join-RepoPath "mql/MQL4/Indicators/OptionX/OptionXFileBridgeSignalExample.ex4") `
    -Required:$RequireMql4

$passed = @($results | Where-Object { $_.Status -eq "passed" }).Count
$skipped = @($results | Where-Object { $_.Status -eq "skipped" }).Count
Write-Host "MetaEditor compile smoke complete: passed=$passed skipped=$skipped"

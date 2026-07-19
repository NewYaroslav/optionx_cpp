param(
    [string]$OutputDir = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "build/metaeditor-smoke-self-test"
}
$OutputDir = (New-Item -ItemType Directory -Force -Path $OutputDir).FullName

$fakeEditor = Join-Path $OutputDir "FakeMetaEditor.exe"
$fakeSourcePath = Join-Path $OutputDir "FakeMetaEditor.cs"
$fakeSource = @'
using System;
using System.IO;
using System.Threading;

public static class Program
{
    public static int Main(string[] args)
    {
        var logPath = "";
        var compilePath = "";
        foreach (var argument in args)
        {
            if (argument.StartsWith("/log:", StringComparison.OrdinalIgnoreCase))
            {
                logPath = argument.Substring(5).Trim('"');
            }
            if (argument.StartsWith("/compile:", StringComparison.OrdinalIgnoreCase))
            {
                compilePath = argument.Substring(9).Trim('"');
            }
        }

        if (string.IsNullOrWhiteSpace(logPath))
        {
            return 3;
        }

        var mode = Environment.GetEnvironmentVariable("OPTIONX_FAKE_METAEDITOR_MODE");
        if (string.Equals(mode, "timeout", StringComparison.OrdinalIgnoreCase))
        {
            Thread.Sleep(TimeSpan.FromSeconds(60));
            return 0;
        }

        if (string.Equals(mode, "fail", StringComparison.OrdinalIgnoreCase))
        {
            File.WriteAllLines(logPath, new[]
            {
                compilePath + " : information: compiling " + compilePath,
                "Result: 1 errors, 0 warnings"
            });
            return 0;
        }

        File.WriteAllLines(logPath, new[]
        {
            compilePath + " : information: compiling " + compilePath,
            "Result: 0 errors, 0 warnings"
        });

        if (!string.Equals(mode, "noartifact", StringComparison.OrdinalIgnoreCase))
        {
            var artifactPath = Path.ChangeExtension(compilePath, ".ex5");
            Directory.CreateDirectory(Path.GetDirectoryName(artifactPath));
            File.WriteAllText(artifactPath, "");
        }

        return 0;
    }
}
'@

Set-Content -Encoding ASCII -LiteralPath $fakeSourcePath -Value $fakeSource

$cscCandidates = @(
    (Join-Path $env:WINDIR "Microsoft.NET\Framework64\v4.0.30319\csc.exe"),
    (Join-Path $env:WINDIR "Microsoft.NET\Framework\v4.0.30319\csc.exe")
)
$csc = $cscCandidates | Where-Object {
    Test-Path -LiteralPath $_ -PathType Leaf
} | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($csc)) {
    throw "Could not find .NET Framework csc.exe to build fake MetaEditor."
}

& $csc `
    /nologo `
    /target:exe `
    "/out:$fakeEditor" `
    $fakeSourcePath | Out-Host
if ($LASTEXITCODE -ne 0) {
    throw "Failed to build fake MetaEditor executable with csc.exe."
}

if (!(Test-Path -LiteralPath $fakeEditor -PathType Leaf)) {
    throw "Failed to build fake MetaEditor executable: $fakeEditor"
}

function Invoke-Smoke {
    param(
        [string]$Name,
        [string]$Mode,
        [bool]$ShouldPass,
        [string]$ExpectedError = ""
    )

    $caseDir = Join-Path $OutputDir $Name
    New-Item -ItemType Directory -Force -Path $caseDir | Out-Null
    $oldMode = $env:OPTIONX_FAKE_METAEDITOR_MODE
    $env:OPTIONX_FAKE_METAEDITOR_MODE = $Mode
    try {
        $actualFailure = $null
        try {
            & (Join-Path $PSScriptRoot "compile-metatrader-mql.ps1") `
                -MetaEditor5Path $fakeEditor `
                -RequireMql5 `
                -OutputDir $caseDir `
                -TimeoutSeconds 2
        } catch {
            $actualFailure = $_
        }

        if ($ShouldPass) {
            if ($null -ne $actualFailure) {
                throw $actualFailure
            }
            Write-Host "$Name passed as expected."
            return
        }

        if ($null -eq $actualFailure) {
            throw "Expected $Name to fail."
        }
        $message = $actualFailure.Exception.Message
        if (![string]::IsNullOrWhiteSpace($ExpectedError) -and
            $message -notlike "*$ExpectedError*") {
            throw "Expected $Name failure to contain '$ExpectedError', got: $message"
        }
        Write-Host "$Name failed as expected: $message"
    } finally {
        $env:OPTIONX_FAKE_METAEDITOR_MODE = $oldMode
    }
}

Invoke-Smoke -Name "success" -Mode "success" -ShouldPass $true
Invoke-Smoke -Name "compile-error" -Mode "fail" -ShouldPass $false -ExpectedError "compile failed with 1 errors"
Invoke-Smoke -Name "missing-artifact" -Mode "noartifact" -ShouldPass $false -ExpectedError "did not produce"
Invoke-Smoke -Name "timeout" -Mode "timeout" -ShouldPass $false -ExpectedError "timed out"

$missingPath = Join-Path $OutputDir "missing-metaeditor.exe"
try {
    & (Join-Path $PSScriptRoot "compile-metatrader-mql.ps1") `
        -MetaEditor5Path $missingPath `
        -RequireMql5 `
        -OutputDir (Join-Path $OutputDir "missing-explicit")
    throw "Expected missing explicit path to fail."
} catch {
    if ($_.Exception.Message -notlike "*Explicit MetaEditor path does not exist*") {
        throw
    }
    Write-Host "missing-explicit failed as expected."
}

Write-Host "MetaEditor compile script self-test passed."

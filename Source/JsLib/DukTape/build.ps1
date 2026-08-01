#Requires -Version 7

[CmdletBinding()]
param(
    [switch]$Rebuild
)

$ErrorActionPreference = 'Stop'
$scriptPath = $PSScriptRoot + '\'

# ---------------------------------------------------------------------------
# Helper functions
# ---------------------------------------------------------------------------

function EscapeParam([string]$Param) {
    if ([string]::IsNullOrWhiteSpace($Param)) {
        return '""'
    }
    if ($Param -match '[ \t"]') {
        return '"' + $Param.Replace('"', '""') + '"'
    }
    return $Param
}

function FindVisualStudio {
    $roots = @(
        [System.Environment]::GetFolderPath('ProgramFilesX86'),
        [System.Environment]::GetFolderPath('ProgramFiles')
    )
    foreach ($root in $roots) {
        $vswhere = Join-Path $root 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            continue
        }
        $vsPath = ""
        try {
            $output = & $vswhere -version '[18.0,19.0)' -requires 'Microsoft.Component.MSBuild' -property installationPath -sort 2>$null
            foreach ($line in $output) {
                $t = $line.Trim()
                if ($t) {
                    $vsPath = $t.EndsWith('\') ? $t : "$t\"
                }
            }
        }
        catch {
            # vswhere failures are non-fatal
        }
        if ($vsPath) {
            return $vsPath
        }
    }
    return ""
}

function FindCmdExe {
    $w = $env:WINDIR
    return ($w.EndsWith('\') ? $w : "$w\") + 'System32\cmd.exe'
}

function FindXcopyExe {
    $w = $env:WINDIR
    return ($w.EndsWith('\') ? $w : "$w\") + 'System32\xcopy.exe'
}

function Invoke-App([string]$CmdLine, [string]$CurFolder, [string]$EnvPath, [bool]$Hide) {
    Write-Host "Running: $CmdLine"

    if ($CmdLine -match '^"([^"]+)"\s*(.*)$') {
        $exe    = $Matches[1]
        $argStr = $Matches[2]
    }
    elseif ($CmdLine -match '^(\S+)\s*(.*)$') {
        $exe    = $Matches[1]
        $argStr = $Matches[2]
    }
    else {
        return 1
    }

    $psi                        = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName               = $exe
    $psi.Arguments              = $argStr
    $psi.UseShellExecute        = $false
    $psi.RedirectStandardOutput = $Hide
    $psi.RedirectStandardError  = $Hide
    if ($CurFolder) {
        $psi.WorkingDirectory = $CurFolder
    }

    $savedPath = $null
    if ($EnvPath) {
        $savedPath = $env:PATH
        $env:PATH  = "$EnvPath;$env:PATH"
        $psi.EnvironmentVariables['PATH'] = $env:PATH
    }

    $proc           = [System.Diagnostics.Process]::new()
    $proc.StartInfo = $psi
    try {
        $proc.Start() | Out-Null
    }
    finally {
        if ($null -ne $savedPath) {
            $env:PATH = $savedPath
        }
    }

    if ($Hide) {
        $outTask = $proc.StandardOutput.ReadToEndAsync()
        $errTask = $proc.StandardError.ReadToEndAsync()
        $proc.WaitForExit()
        $outTask.GetAwaiter().GetResult() | Out-Null
        $errTask.GetAwaiter().GetResult() | Out-Null
    }
    else {
        $proc.WaitForExit()
    }

    return $proc.ExitCode
}

function CheckForNewerFile([string]$File, [datetime]$BuildDate) {
    if (-not (Test-Path -LiteralPath $File)) {
        return $false
    }
    return (Get-Item -LiteralPath $File).LastWriteTime -gt $BuildDate
}

function CheckForNewerFiles([string]$Folder, [datetime]$BuildDate) {
    foreach ($sub in Get-ChildItem -LiteralPath $Folder -Directory -ErrorAction SilentlyContinue) {
        if (CheckForNewerFiles $sub.FullName $BuildDate) {
            return $true
        }
    }
    foreach ($f in Get-ChildItem -LiteralPath $Folder -File -ErrorAction SilentlyContinue) {
        if (CheckForNewerFile $f.FullName $BuildDate) {
            Write-Host "File: `"$($f.FullName)`" is newer... rebuilding"
            return $true
        }
    }
    return $false
}

function GetLowestFileTimestamp([string[]]$Files) {
    $lowest = $null
    foreach ($f in $Files) {
        if (-not (Test-Path -LiteralPath $f)) {
            return $null
        }
        $ts = (Get-Item -LiteralPath $f).LastWriteTime
        if ($null -eq $lowest -or $ts -lt $lowest) {
            $lowest = $ts
        }
    }
    return $lowest
}

function FixEOL([string]$FileName) {
    try {
        $reader = [System.IO.StreamReader]::new($FileName)
    }
    catch {
        return 1
    }

    $content = $null
    try {
        $content = $reader.ReadToEnd()
    }
    catch {
        $reader.Dispose()
        return 1
    }
    finally {
        $reader.Dispose()
    }

    # Normalize all line endings to CRLF
    $content = $content.Replace("`r`n", "`n")
    $content = $content.Replace("`r",   "`n")
    $content = $content.Replace("`n",   "`r`n")
    if (-not $content.EndsWith("`r`n")) {
        $content += "`r`n"
    }

    try {
        $writer = [System.IO.StreamWriter]::new($FileName, $false)
    }
    catch {
        return 1
    }

    try {
        $writer.Write($content)
        return 0
    }
    catch {
        return 1
    }
    finally {
        $writer.Dispose()
    }
}

# ---------------------------------------------------------------------------
# Argument validation
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# Locate Visual Studio
# ---------------------------------------------------------------------------

$vsPath = FindVisualStudio
if (-not $vsPath) {
    Write-Host "Error: Unable to locate Microsoft Visual Studio 2026."
    exit 1
}

# ---------------------------------------------------------------------------
# Setup paths
# ---------------------------------------------------------------------------

$pythonPath = $scriptPath + '..\..\..\Utilities\Python27'
$cmdExe     = FindCmdExe
$xcopyExe = FindXcopyExe

# ---------------------------------------------------------------------------
# Check if rebuild is needed
# ---------------------------------------------------------------------------

$doRebuild = [bool]$Rebuild
if (-not $doRebuild) {
    Write-Host "Checking if source files were modified..."
    $stamps = @(
        $scriptPath + '..\..\..\Include\JsLib\DukTape\duk_config.h',
        $scriptPath + '..\..\..\Include\JsLib\DukTape\duktape.h',
        $scriptPath + 'Source\dist\duktape.c',
        $scriptPath + 'Source\dist\duktape.h',
        $scriptPath + 'Source\dist\duk_config.h',
        $scriptPath + 'Source\dist\duk_source_meta.json'
    )
    $buildDate = GetLowestFileTimestamp $stamps
    if ($null -eq $buildDate -or
        (CheckForNewerFile  ($scriptPath + 'duk_custom.h') $buildDate) -or
        (CheckForNewerFile  ($scriptPath + 'build.ps1')    $buildDate) -or
        (CheckForNewerFiles ($scriptPath + 'Source\config')     $buildDate) -or
        (CheckForNewerFiles ($scriptPath + 'Source\src-input')  $buildDate)) {
        $doRebuild = $true
    }
}

if (-not $doRebuild) {
    Write-Host "Distributable files are up-to-date"
    exit 0
}

# ---------------------------------------------------------------------------
# Rebuild
# ---------------------------------------------------------------------------

Write-Host "Building..."

[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C RD /S /Q ' + (EscapeParam ($scriptPath + 'Source\dist'))) ($scriptPath + 'Source') '' $true)
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD '       + (EscapeParam ($scriptPath + 'Source\dist'))) ($scriptPath + 'Source') '' $true)

$cmd  = (EscapeParam ($pythonPath + '\python.exe')) + ' '
$cmd += (EscapeParam ($scriptPath + 'Source\tools\configure.py')) + ' '
$cmd += '--fixup-file '        + (EscapeParam ($scriptPath + 'duk_custom.h'))   + ' '
$cmd += '--output-directory '  + (EscapeParam ($scriptPath + 'Source\dist'))
$err  = Invoke-App $cmd ($scriptPath + 'Source') $pythonPath $false
if ($err -ne 0) {
    Write-Host "Errors detected while building distributable files."
    exit $err
}

Start-Sleep -Seconds 5  # Python processes may still be writing files

Write-Host "Fixing line endings..."
$err = FixEOL ($scriptPath + 'Source\dist\duk_config.h')
if ($err -eq 0) {
    $err = FixEOL ($scriptPath + 'Source\dist\duktape.h')
}
if ($err -ne 0) {
    Write-Host "Errors found."
    exit $err
}

$includeDir = $scriptPath + '..\..\..\Include\JsLib\DukTape'
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD ' + (EscapeParam $includeDir)) ($scriptPath + 'Source') '' $true)
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C COPY /Y ' + (EscapeParam ($scriptPath + 'Source\dist\duk_config.h')) + ' ' + (EscapeParam $includeDir)) `
    ($scriptPath + 'Source') '' $false)
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C COPY /Y ' + (EscapeParam ($scriptPath + 'Source\dist\duktape.h'))    + ' ' + (EscapeParam $includeDir)) `
    ($scriptPath + 'Source') '' $false)

Write-Host "Done!"
exit 0

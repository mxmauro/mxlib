#Requires -Version 7

[CmdletBinding()]
param()

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

function FindRobocopyExe {
    $w = $env:WINDIR
    return ($w.EndsWith('\') ? $w : "$w\") + 'System32\Robocopy.exe'
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

function CopyFiles([string]$SrcFolder, [string]$DestFolder, [string]$FileMask) {
    $robocopyExe = FindRobocopyExe
    $cmd  = (EscapeParam $robocopyExe) + ' '
    $cmd += (EscapeParam $SrcFolder) + ' '
    $cmd += (EscapeParam $DestFolder)
    if ($FileMask) {
        $cmd += ' ' + $FileMask
    }
    $cmd += ' /COPY:DAT /XO /NDL /NJH /NJS /NP /NS /NC /E'
    $err = Invoke-App $cmd $script:scriptPath '' $true
    if ($err -lt 8) {
        $err = 0
    }
    return $err
}

function CreateIncludeAll {
    $fileName  = $script:scriptPath + '..\..\Include\RapidJSON\rapidjson-all.h'
    $doRebuild = $false

    if (Test-Path -LiteralPath $fileName) {
        $buildDate = (Get-Item -LiteralPath $fileName).LastWriteTime
        if ((CheckForNewerFiles ($script:scriptPath + 'Source\include\rapidjson') $buildDate) -or
            (CheckForNewerFile  ($script:scriptPath + 'build.ps1') $buildDate)) {
            $doRebuild = $true
        }
    }
    else {
        $doRebuild = $true
    }

    if (-not $doRebuild) {
        return 0
    }

    Write-Host "Rebuilding rapidjson-all.h..."

    try {
        $writer = [System.IO.StreamWriter]::new($fileName, $false, [System.Text.Encoding]::ASCII)
    }
    catch {
        return 1
    }

    try {
        $content = @'
#ifndef _RAPIDJSON_INCLUDEALL_H
#define _RAPIDJSON_INCLUDEALL_H

#include <stdexcept>

#define RAPIDJSON_ASSERT(x) if (!(x)) throw std::runtime_error(#x)
#define RAPIDJSON_NOEXCEPT_ASSERT(x)
#define RAPIDJSON_DEFAULT_ALLOCATOR MemoryPoolAllocator<CMxLibAllocator>

//------------------------------------------------------------------------------

namespace rapidjson {

class CMxLibAllocator
{
public:
  static const bool kNeedFree = true;

  void* Malloc(size_t size)
    {
    return MX_MALLOC(size);
    };

  void* Realloc(void *originalPtr, size_t originalSize, size_t newSize)
    {
    UNREFERENCED_PARAMETER(originalSize);
    return MX_REALLOC(originalPtr, newSize);
    };

  static void Free(void *ptr)
    {
    MX_FREE(ptr);
    return;
    };
};

} //namespace rapidjson

//------------------------------------------------------------------------------

#pragma warning(disable : 26495)
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "memorybuffer.h"
#pragma warning(default : 26495)

//------------------------------------------------------------------------------

namespace rapidjson {

__inline const Value* LookupMember(_In_ const Value &parent, _In_z_ LPCSTR szMemberNameA)
{
  const Value::ConstMemberIterator &member = parent.FindMember(szMemberNameA);
  return (member != parent.MemberEnd()) ? &(member->value) : NULL;
}

__inline const Value& LookupMemberRef(_In_ const Value &parent, _In_z_ LPCSTR szMemberNameA)
{
  static const Value nullValue;
  const Value::ConstMemberIterator &member = parent.FindMember(szMemberNameA);
  return (member != parent.MemberEnd()) ? member->value : nullValue;
}

} //namespace rapidjson

//------------------------------------------------------------------------------

#define RAPIDJSON_TRY try

#define RAPIDJSON_CATCH(hr)           \
    catch (std::bad_alloc &e)         \
    {                                 \
      UNREFERENCED_PARAMETER(e);      \
      hr = E_OUTOFMEMORY;             \
    }                                 \
    catch (std::runtime_error &e)     \
    {                                 \
      UNREFERENCED_PARAMETER(e);      \
      hr = MX_E_InvalidData;          \
    }                                 \
    catch (...)                       \
    {                                 \
      hr = MX_E_UnhandledException;   \
    }

#define RAPIDJSON_CATCH_RETURN        \
    catch (std::bad_alloc &e)         \
    {                                 \
      UNREFERENCED_PARAMETER(e);      \
      return E_OUTOFMEMORY;           \
    }                                 \
    catch (std::runtime_error &e)     \
    {                                 \
      UNREFERENCED_PARAMETER(e);      \
      return MX_E_InvalidData;        \
    }                                 \
    catch (...)                       \
    {                                 \
      return MX_E_UnhandledException; \
    }

//------------------------------------------------------------------------------

#endif //_RAPIDJSON_INCLUDEALL_H
'@
        # Normalize line endings to CRLF
        $content = $content.Replace("`r`n", "`n").Replace("`r", "`n").Replace("`n", "`r`n")
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

$cmdExe = FindCmdExe

# ---------------------------------------------------------------------------
# Copy files
# ---------------------------------------------------------------------------

[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD ' + (EscapeParam ($scriptPath + '..\..\Include\RapidJSON'))) '' '' $true)

$err = CopyFiles ($scriptPath + 'Source\include\rapidjson') ($scriptPath + '..\..\Include\RapidJSON') '*'
if ($err -ne 0) {
    Write-Host "Errors detected while copying files. [$err]"
    exit $err
}

# ---------------------------------------------------------------------------
# Create include-all header
# ---------------------------------------------------------------------------

$err = CreateIncludeAll
if ($err -ne 0) {
    Write-Host "Errors detected while creating the include-all file. [$err]"
    exit $err
}

Write-Host "RapidJSON files are up-to-date"
exit 0

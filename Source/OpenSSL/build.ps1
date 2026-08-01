#Requires -Version 7

[CmdletBinding()]
param(
    [string]$Configuration = "",
    [string]$Platform = "",
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
        $ext = $f.Extension.ToLower()
        if ($ext -in '.cpp', '.c', '.h', '.in') {
            if ((CheckForNewerFile $f.FullName $BuildDate) -and
                -not (Test-Path -LiteralPath ($f.FullName + '.in'))) {
                Write-Host "File: `"$($f.FullName)`" is newer... rebuilding"
                return $true
            }
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

function CreateConfiguration {
    $re1 = [regex]'^\s*my\s+%targets\s+=\s+\(\s*'
    $re2 = [regex]'^\s*\);\s*'
    $re3 = [regex]'^\s*"VC-[^"]*"\s*=>\s*\{\s*'
    $re4 = [regex]'^\s*\},\s*'

    $inFile  = $script:scriptPath + 'Source\Configurations\10-main.conf'
    $outFile = $script:scriptPath + 'Temp\compiler_config.conf'

    try {
        $reader = [System.IO.StreamReader]::new($inFile)
    }
    catch {
        return 1
    }
    try {
        $writer = [System.IO.StreamWriter]::new($outFile, $false)
    }
    catch {
        $reader.Dispose()
        return 1
    }

    try {
        $writer.Write("my %targets = (`r`n")
        $area = 0
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($area -eq 0) {
                if ($re1.IsMatch($line)) {
                    $area = 1
                }
            }
            elseif ($area -eq 1) {
                if ($re2.IsMatch($line)) {
                    break
                }
                if ($re3.IsMatch($line)) {
                    $name = $line.Substring($line.IndexOf('VC-'))
                    $name = $name.Substring(0, $name.IndexOf('"'))
                    if ($name -eq 'VC-noCE-common') {
                        $writer.Write("    `"base-$name`" => {`r`n")
                        $writer.Write(@'
        inherit_from     => [ "base-VC-common" ],
        cflags           => add(picker(default => "-DUNICODE -D_UNICODE ",
                                       debug   => sub {
                                           ($disabled{shared} ? "/MTd" : "/MDd") . " /Od -DDEBUG -D_DEBUG";
                                       },
                                       release => sub {
                                           ($disabled{shared} ? "/MT" : "/MD") . " /O2";
                                       })),
        lib_cflags       => add(picker(debug   => sub {
                                           ($disabled{shared} ? "/MTd" : "/MDd");
                                       },
                                       release => sub {
                                           ($disabled{shared} ? "/MT" : "/MD");
                                       })),
        bin_cflags       => add(picker(debug   => sub {
                                           ($disabled{shared} ? "/MTd" : "/MDd");
                                       },
                                       release => sub {
                                           ($disabled{shared} ? "/MT" : "/MD");
                                       })),
        bin_lflags       => add("/subsystem:console /opt:ref"),
        ex_libs          => add(sub {
            my @ex_libs = ();
            push @ex_libs, 'ws2_32.lib' unless $disabled{sock};
            push @ex_libs, 'gdi32.lib advapi32.lib crypt32.lib user32.lib';
            return join(" ", @ex_libs);
        }),
    },
'@)
                        $area = 2
                    }
                    else {
                        $writer.Write("    `"base-$name`" => {`r`n")
                        $area = 3
                    }
                }
            }
            elseif ($area -eq 2) {
                if ($re4.IsMatch($line)) {
                    $area = 1
                }
            }
            elseif ($area -eq 3) {
                if ($re4.IsMatch($line)) {
                    $writer.Write("$line`r`n")
                    $area = 1
                }
                else {
                    $i = $line.IndexOf('"VC-')
                    while ($i -ge 0) {
                        $line = $line.Substring(0, $i + 1) + 'base-' + $line.Substring($i + 1)
                        $i    = $line.IndexOf('"VC-', $i + 7)
                    }
                    $writer.Write("$line`r`n")
                }
            }
        }

        $writer.Write(@'
    "my-VC-WIN32" => {
        inherit_from     => [ "base-VC-WIN32" ],
        cflags           => add("-wd4244 -wd4267"),
        lflags           => add("/ignore:4221 LIBCMT.lib")
    },
    "my-VC-WIN64A" => {
        inherit_from     => [ "base-VC-WIN64A" ],
        cflags           => add("-wd4244 -wd4267"),
        lflags           => add("/ignore:4221 LIBCMT.lib")
    },
    "debug-my-VC-WIN32" => {
        inherit_from     => [ "base-VC-WIN32" ],
        cflags           => add("-wd4244 -wd4267"),
        lflags           => add("/ignore:4221 LIBCMTD.lib")
    },
    "debug-my-VC-WIN64A" => {
        inherit_from     => [ "base-VC-WIN64A" ],
        cflags           => add("-wd4244 -wd4267"),
        lflags           => add("/ignore:4221 LIBCMTD.lib")
    }
);
'@)
        return 0
    }
    catch {
        return 1
    }
    finally {
        $writer.Dispose()
        $reader.Dispose()
    }
}

function Patch_RAND_WIN_C {
    $re1     = [regex]'^\s*#\s*define\s+USE_BCRYPTGENRANDOM\s*'
    $inFile  = $script:scriptPath + 'Source\providers\implementations\rands\seeding\rand_win.c'
    $outFile = $script:scriptPath + 'Temp\rand_win.c'

    try {
        $reader = [System.IO.StreamReader]::new($inFile)
    }
    catch {
        return 1
    }
    try {
        $writer = [System.IO.StreamWriter]::new($outFile, $false)
    }
    catch {
        $reader.Dispose()
        return 1
    }

    try {
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            if ($re1.IsMatch($line)) {
                $line = ""
            }
            $writer.Write("$line`r`n")
        }
        return 0
    }
    catch {
        return 1
    }
    finally {
        $writer.Dispose()
        $reader.Dispose()
    }
}

function Patch_Makefile {
    $inFile      = $script:scriptPath + 'Source\makefile'
    $outFile     = $script:scriptPath + 'Temp\makefile'
    $randWinPath = 'providers\implementations\rands\seeding'

    try {
        $reader = [System.IO.StreamReader]::new($inFile)
    }
    catch {
        return 1
    }
    try {
        $writer = [System.IO.StreamWriter]::new($outFile, $false)
    }
    catch {
        $reader.Dispose()
        return 1
    }

    try {
        while (-not $reader.EndOfStream) {
            $line = $reader.ReadLine()
            $line = $line.Replace('libcrypto.lib',            'openssl_libcrypto.lib')
            $line = $line.Replace('libssl.lib',               'openssl_libssl.lib')
            $line = $line.Replace('providers\libcommon.lib',  'openssl_libcommon.lib')
            $line = $line.Replace('providers\libdefault.lib', 'openssl_libdefault.lib')
            $line = $line.Replace('providers\liblegacy.lib',  'openssl_liblegacy.lib')
            $line = $line.Replace('/Fdossl_static.pdb',       '/Fdopenssl_static.pdb')

            if ($line.StartsWith('LIBS=')) {
                $parts = ($line.Substring(5) -split '\s+') |
                         Where-Object { $_ -and -not $_.StartsWith('apps\') -and -not $_.StartsWith('test\') }
                $line  = 'LIBS= ' + ($parts -join ' ')
            }

            $needle = "$randWinPath\rand_win.c"
            $pos    = $line.IndexOf($needle, [System.StringComparison]::OrdinalIgnoreCase)
            if ($pos -ge 0) {
                $line = $line.Substring(0, $pos) + '..\Temp' + $line.Substring($pos + $randWinPath.Length)
            }

            $writer.Write("$line`r`n")
        }
        return 0
    }
    catch {
        return 1
    }
    finally {
        $writer.Dispose()
        $reader.Dispose()
    }
}

# ---------------------------------------------------------------------------
# Argument validation
# ---------------------------------------------------------------------------

if (-not $Configuration -and -not $Platform -and -not $Rebuild.IsPresent) {
    Write-Host "Use: .\build.ps1 -Configuration (Debug|Release) -Platform (x86|x64) [-Rebuild]"
    exit 1
}

switch ($Configuration.ToLower()) {
    'debug' {
        $Configuration = 'Debug'
        $isDebug       = 'debug'
        $configDebug   = 'debug-'
        $defineNoErr   = '--debug'
        break
    }
    'release' {
        $Configuration = 'Release'
        $isDebug       = ''
        $configDebug   = ''
        $defineNoErr   = 'no-err'
        break
    }
    '' {
        Write-Host "Error: -Configuration parameter not specified."
        exit 1
    }
    default {
        Write-Host "Error: Invalid configuration specified."
        exit 1
    }
}

switch ($Platform.ToLower()) {
    'x86' {
        $Platform            = 'x86'
        $platformPath        = 'Win32'
        $configurationTarget = 'my-VC-WIN32'
        break
    }
    'x64' {
        $Platform            = 'x64'
        $platformPath        = 'x64'
        $configurationTarget = 'my-VC-WIN64A'
        break
    }
    '' {
        Write-Host "Error: -Platform parameter not specified."
        exit 1
    }
    default {
        Write-Host "Error: Invalid platform specified."
        exit 1
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

$perlPath = $scriptPath + '..\..\Utilities\Perl5\bin'
$nasmPath = $scriptPath + '..\..\Utilities\Nasm'
$cmdExe   = FindCmdExe
$xcopyExe = FindXcopyExe

if ($Platform -eq 'x86') {
    $vcVars = $vsPath + 'VC\Auxiliary\Build\vcvarsamd64_x86.bat'
}
else {
    $vcVars = $vsPath + 'VC\Auxiliary\Build\vcvars64.bat'
}

# ---------------------------------------------------------------------------
# Check if rebuild is needed
# ---------------------------------------------------------------------------

$doRebuild = [bool]$Rebuild
if (-not $doRebuild) {
    Write-Host "Checking if source files were modified..."
    $libBase = $scriptPath + "..\..\Libs\$platformPath\$Configuration\"
    $stamps  = @(
        $libBase + 'openssl_libcrypto.lib', $libBase + 'openssl_libssl.lib',
        $libBase + 'openssl_libcommon.lib', $libBase + 'openssl_libdefault.lib',
        $libBase + 'openssl_liblegacy.lib', $libBase + 'openssl_static.pdb'
    )
    $buildDate = GetLowestFileTimestamp $stamps
    if ($null -eq $buildDate -or
        (CheckForNewerFiles ($scriptPath + 'Source') $buildDate) -or
        (CheckForNewerFile  ($scriptPath + 'build.ps1') $buildDate)) {
        $doRebuild = $true
    }
}

if (-not $doRebuild) {
    Write-Host "Libraries are up-to-date"
    exit 0
}

# ---------------------------------------------------------------------------
# Rebuild
# ---------------------------------------------------------------------------

Write-Host "Creating configuration settings..."

[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD ' + (EscapeParam ($scriptPath + 'Temp'))) '' '' $true)

$err = CreateConfiguration
if ($err -ne 0) {
    Write-Host "Errors found!"
    exit $err
}

Write-Host "Creating makefile..."
$cmd  = "perl.exe Configure $configDebug$configurationTarget $defineNoErr"
$cmd += " no-asm no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared no-srp no-engine no-module"
$cmd += " -DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG"  # -DOPENSSL_NO_DEPRECATED -DOPENSSL_API_COMPAT=30000
$cmd += " -DUNICODE -D_UNICODE"
if (-not $isDebug) {
    $cmd += " -DOPENSSL_NO_FILENAMES"
}
$cmd += " " + (EscapeParam "--config=${scriptPath}Temp\compiler_config.conf") + " "
$err  = Invoke-App $cmd ($scriptPath + 'Source') ($perlPath + ';' + $nasmPath) $false
if ($err -ne 0) {
    Write-Host "Errors found!"
    exit $err
}

Write-Host "Patching makefile..."
$err = Patch_Makefile
if ($err -ne 0) {
    Write-Host "Errors found!"
    exit $err
}

[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MOVE /Y ' +
    (EscapeParam ($scriptPath + 'Temp\makefile')) + ' ' +
    (EscapeParam ($scriptPath + 'Source\makefile'))) '' '' $false)

Write-Host "Patching rand_win.c..."
$err = Patch_RAND_WIN_C
if ($err -ne 0) {
    Write-Host "Errors found!"
    exit $err
}

# Clean before compile (errors ignored)
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C "CALL ' + (EscapeParam $vcVars) + ' && nmake.exe clean"') `
    ($scriptPath + 'Source') ($perlPath + ';' + $nasmPath) $true)

Write-Host "Compiling..."
$err = Invoke-App ((EscapeParam $cmdExe) + ' /S /C "CALL ' + (EscapeParam $vcVars) + ' && nmake.exe /S build_libs"') `
    ($scriptPath + 'Source') ($perlPath + ';' + $nasmPath) $false
if ($err -ne 0) {
    Write-Host "Errors found!"
    exit $err
}

Start-Sleep -Seconds 5  # NMake processes may still be writing libs (see original build.vbs)

# Copy include files
$genBase = $scriptPath + "Generated\include\$platformPath\$Configuration"
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C RD /S /Q ' + (EscapeParam $genBase)) '' '' $true)
foreach ($d in @(
    $scriptPath + 'Generated',
    $scriptPath + 'Generated\include',
    $scriptPath + "Generated\include\$platformPath",
    $genBase
)) {
    [void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD ' + (EscapeParam $d)) '' '' $true)
}

[void](Invoke-App ((EscapeParam $xcopyExe) + ' ' +
    (EscapeParam ($scriptPath + 'Source\include\*')) + ' ' +
    (EscapeParam ($genBase + '\')) + ' /E /Q /-Y') '' '' $true)

# Move library files
$libBase = $scriptPath + "..\..\Libs\$platformPath\$Configuration"
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C RD /S /Q ' + (EscapeParam ($libBase + '\OpenSSL'))) '' '' $true)
foreach ($d in @(
    $scriptPath + '..\..\Libs',
    $scriptPath + "..\..\Libs\$platformPath",
    $libBase,
    $libBase + '\OpenSSL'
)) {
    [void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MD ' + (EscapeParam $d)) '' '' $true)
}

foreach ($f in @(
    'openssl_libcrypto.lib', 'openssl_libssl.lib', 'openssl_libcommon.lib',
    'openssl_libdefault.lib', 'openssl_liblegacy.lib', 'openssl_static.pdb'
)) {
    [void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C MOVE /Y ' +
        (EscapeParam ($scriptPath + "Source\$f")) + ' ' +
        (EscapeParam ("$libBase\$f")) + '  >NUL 2>NUL') '' '' $false)
}

# Clean after compile
[void](Invoke-App ((EscapeParam $cmdExe) + ' /S /C "CALL ' + (EscapeParam $vcVars) + ' && nmake.exe clean"') `
    ($scriptPath + 'Source') ($perlPath + ';' + $nasmPath) $true)

Write-Host "Done!"
exit 0

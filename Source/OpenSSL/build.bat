@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

SET _PLATFORM=
SET _PLATFORM2=
SET _ISDEBUG=
SET _CONFIGTARGET=
SET _CONFIGDEBUG=
SET _VSTOOLS=%VS120COMNTOOLS%

:parse_cmdline
IF /I [%1] == [x86] (
    IF NOT [%_PLATFORM%] == [] GOTO invalid_cmdline
    SET _PLATFORM=x86
    SET _DESTPATH=Win32
    SET _CONFIGTARGET=my-VC-WIN32
) ELSE IF /I [%1] == [x64] (
    IF NOT [%_PLATFORM%] == [] GOTO invalid_cmdline
    SET _PLATFORM=x64
    SET _DESTPATH=x64
    SET _CONFIGTARGET=my-VC-WIN64A
) ELSE IF /I [%1] == [debug] (
    IF NOT [%_ISDEBUG%] == [] GOTO invalid_cmdline
    SET _ISDEBUG=debug
    SET _CONFIGDEBUG=debug-
) ELSE (
    GOTO invalid_cmdline
)
SHIFT
IF NOT [%1] == [] GOTO parse_cmdline

IF [%_PLATFORM%] == [] GOTO invalid_cmdline
ECHO Setting up target %_CONFIGDEBUG%%_CONFIGTARGET%...


REM Setup environment
IF "%_VSTOOLS%" == "" GOTO show_err
CALL "%_VSTOOLS%\..\..\VC\vcvarsall.bat" %_PLATFORM%
IF "%_VSTOOLS%" == "" GOTO err_cantsetupvs

IF NOT [%_ISDEBUG%] == [] (
    SET _DESTPATH=%_DESTPATH%\Debug
    SET _DEFINE_NOERR=--debug
) ELSE (
    SET _DESTPATH=%_DESTPATH%\Release
    SET _DEFINE_NOERR=no-err
)
SET _OBJDIR=%~dp0..\..\obj\%_DESTPATH%\OpenSSL
SET _LIBDIR=%~dp0..\..\Libs\%_DESTPATH%

SET PATH=%~dp0perl\bin;%~dp0nasm;%PATH%
SET PERL5LIB=%~dp0perl\lib


REM Cleanup some temporary folders
RD /S /Q "%~dp0Source\inc32" >NUL 2>NUL
RD /S /Q "%_OBJDIR%" >NUL 2>NUL
RD /S /Q "%~dp0Source\out32" >NUL 2>NUL
RD /S /Q "%~dp0Source\out32.dbg" >NUL 2>NUL
MD "%_LIBDIR%" >NUL 2>NUL
MD "%_OBJDIR%" >NUL 2>NUL


REM Prepare
PUSHD "%~dp0Source"
ECHO Configuring...
perl.exe Configure %_CONFIGDEBUG%%_CONFIGTARGET% %_DEFINE_NOERR% no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia -DOPENSSL_NO_DGRAM -DOPENSSL_NO_OCSP -DUNICODE -D_UNICODE "--prefix=%_LIBDIR%" "-FI%~dp0..\config.h" "--config=%~dp0..\compiler_config.conf"
IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare


REM From ms\do_ms.bat and ms\do_win64a.bat
ECHO Generating makefiles...

TYPE util\mkfiles.pl | FINDSTR /v "\"test\"," | FINDSTR /v "\"apps\"," >util\mkfiles2.pl
perl.exe util\mkfiles2.pl >MINFO
DEL /Q util\mkfiles2.pl
IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare

IF [%_PLATFORM%] == [x86] (
    perl.exe util\mk1mf.pl nasm "TMP=%_OBJDIR%" VC-WIN32 >ms\nt.mak
    IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare
) ELSE (
    perl.exe ms\uplink-x86_64.pl masm > ms\uptable.asm
    "%VCINSTALLDIR%\bin\x86_amd64\ML64.exe" -c -Foms\uptable.obj ms\uptable.asm
    IF NOT %ERRORLEVEL% == 0 GOTO bad_compile
    perl.exe util\mk1mf.pl nasm "TMP=%_OBJDIR%" VC-WIN64A >ms\nt.mak
    IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare
)

CSCRIPT "..\find_and_replace.vbs" //nologo ms\nt.mak "$(FIPS_SHA1_EXE) lib exe" "$(FIPS_SHA1_EXE) lib" >ms\nt2.mak
MOVE /Y ms\nt2.mak ms\nt.mak

perl.exe util\mkdef.pl 32 libeay >ms\libeay32.def
IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare
perl.exe util\mkdef.pl 32 ssleay >ms\ssleay32.def  
IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare

REM Fix buildinf.h
ECHO #define DATE "%date% %time%" >%~dp0crypto\buildinf.h

REM Compile
nmake -f ms\nt.mak
IF NOT %ERRORLEVEL% == 0 GOTO bad_compile
POPD

REM Move needed files
IF NOT [%_ISDEBUG%] == [] (
    MOVE "%~dp0Source\out32.dbg\libeay32.lib" "%_LIBDIR%\libeay32.lib" >NUL 2>NUL
    MOVE "%~dp0Source\out32.dbg\ssleay32.lib" "%_LIBDIR%\ssleay32.lib" >NUL 2>NUL
) ELSE (
    MOVE "%~dp0Source\out32\libeay32.lib" "%_LIBDIR%\libeay32.lib" >NUL 2>NUL
    MOVE "%~dp0Source\out32\ssleay32.lib" "%_LIBDIR%\ssleay32.lib" >NUL 2>NUL
)

REM Cleanup
RD /S /Q "%~dp0Source\out32" >NUL 2>NUL
RD /S /Q "%~dp0Source\out32.dbg" >NUL 2>NUL
DEL /Q "%_OBJDIR%\*.c" >NUL 2>NUL
DEL /Q "%_OBJDIR%\*.h" >NUL 2>NUL

ENDLOCAL
GOTO end

:show_err
ECHO Please ensure Visual Studio is installed
PAUSE
GOTO end_with_error

:err_cantsetupvs
ENDLOCAL
ECHO Cannot initialize Visual Studio Command Prompt environment
PAUSE
GOTO end_with_error

:bad_compile
POPD
ENDLOCAL
ECHO Errors detected while compiling project
PAUSE
GOTO end_with_error

:bad_prepare
POPD
ENDLOCAL
ECHO Errors detected while preparing makefiles
PAUSE
GOTO end_with_error

:invalid_cmdline
ECHO Invalid command line. Use: BUILD.BAT x86^|x64 [debug]
PAUSE
:end_with_error
EXIT /B 1

:end
EXIT /B 0

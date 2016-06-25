@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

IF [%_PERLPATH%] == [] (
    ECHO Please set _PERLPATH environment variable to point where Perl 5 is located
    PAUSE
    GOTO end_with_error
)
IF [%_NASMPATH%] == [] (
    ECHO Please set _NASMPATH environment variable to point where Netwide Assembler is located
    PAUSE
    GOTO end_with_error
)

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

SET PATH=%_PERLPATH%\bin;%_NASMPATH%\;%PATH%

REM Prepare
PUSHD "%~dp0Source"
ECHO Configuring...
perl.exe Configure %_CONFIGDEBUG%%_CONFIGTARGET% %_DEFINE_NOERR% no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared -DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG -DUNICODE -D_UNICODE "--config=%~dp0..\compiler_config.conf"
REM  -DOPENSSL_NO_OCSP "--prefix=%_LIBDIR%" "-FI%~dp0..\config.h"
IF NOT %ERRORLEVEL% == 0 GOTO bad_prepare

REM Clean before compile
NMAKE clean >NUL 2>NUL

REM Compile
NMAKE
IF NOT %ERRORLEVEL% == 0 GOTO bad_compile

REM Pause 5 seconds because NMake's processes may still creating the lib WTF?????
PING 127.0.0.1 -n 6 >NUL

REM Move needed files
MD "%_LIBDIR%" >NUL 2>NUL
MD "%_OBJDIR%" >NUL 2>NUL
MOVE /Y "%~dp0libssl.lib"      "%_LIBDIR%"
MOVE /Y "%~dp0libcrypto.lib"   "%_LIBDIR%"
MOVE /Y "%~dp0ossl_static.pdb" "%_LIBDIR%"
MD "%~dp0..\Generated" >NUL 2>NUL
MD "%~dp0..\Generated\OpenSSL" >NUL 2>NUL
COPY /Y "%~dp0include\openssl\opensslconf.h" "%~dp0..\Generated\OpenSSL"

REM Clean after compile
NMAKE clean >NUL 2>NUL

POPD
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

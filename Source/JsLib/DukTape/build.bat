@ECHO OFF
SETLOCAL ENABLEEXTENSIONS

IF [%_PYTHONPATH%] == [] (
    ECHO Please set _PYTHONPATH environment variable to point where Python 2.x is located
    PAUSE
    GOTO end_with_error
)

SET PATH=%_PYTHONPATH%;%PATH%

REM Prepare
PUSHD "%~dp0Source"

REM Build
python.exe util\make_dist.py
IF NOT %ERRORLEVEL% == 0 GOTO bad_build

REM Copy needed files
MD "%~dp0..\..\..\Include\JsLib\DukTape" >NUL 2>NUL
COPY /Y "%~dp0Source\dist\src\duk_config.h"  "%~dp0..\..\..\Include\JsLib\DukTape"
COPY /Y "%~dp0Source\dist\src\duktape.h"     "%~dp0..\..\..\Include\JsLib\DukTape"

POPD
ENDLOCAL
GOTO end

:bad_build
POPD
ENDLOCAL
ECHO Errors detected while building project
PAUSE
GOTO end_with_error

:end_with_error
EXIT /B 1

:end
EXIT /B 0

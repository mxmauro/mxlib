@ECHO OFF
CALL "%~dp0build.bat" x86 debug
IF NOT %ERRORLEVEL% == 0 GOTO end
CALL "%~dp0build.bat" x86
IF NOT %ERRORLEVEL% == 0 GOTO end
CALL "%~dp0build.bat" x64 debug
IF NOT %ERRORLEVEL% == 0 GOTO end
CALL "%~dp0build.bat" x64
IF NOT %ERRORLEVEL% == 0 GOTO end
:end
@ECHO OFF
REM %1 has $(TargetDir), %2 has $(PlatformTarget)

MD "%~1\Web" >NUL 2>NUL
MD "%~1\Web\Certificates" >NUL 2>NUL

COPY /Y "%~dp0Certificates\ssl.crt" "%~1\Web\Certificates"
COPY /Y "%~dp0Certificates\ssl.key" "%~1\Web\Certificates"

ROBOCOPY "%~dp0Web" "%~1\Web" /COPYALL /IS /IT /XJ /E /XF *.~* /NP >NUL 2>NUL
REM Resetting ROBOCOPY errorlevel to zero
CMD /c "EXIT /b 0"

COPY /Y "%~dp0MySQL\%~2\libmysql.dll" "%~1"

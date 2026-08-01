@ECHO OFF
pwsh build.ps1 -configuration debug -platform x86 -rebuild
IF errorlevel 1 EXIT /b %errorlevel%
pwsh build.ps1 -configuration release -platform x86 -rebuild
IF errorlevel 1 EXIT /b %errorlevel%
EXIT /b 0

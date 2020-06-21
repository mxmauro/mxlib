@ECHO OFF
REM %1 has $(TargetDir), %2 has $(PlatformTarget)

MD "%~1\Web" >NUL 2>NUL

ROBOCOPY "%~dp0Web" "%~1\Web" /COPY:DAT /IS /IT /XJ /E /XF *.~* /NP >NUL 2>NUL
REM Resetting ROBOCOPY errorlevel to zero
CMD /c "EXIT /b 0"

"%~dp0..\..\SupportFiles\7za.exe" -y -o"%~1" e "%~dp0..\..\SupportFiles\MariaDB 3_1_8\MariaDB_%~2.zip" libmariadb.dll libmariadb.pdb pvio_shmem.dll pvio_shmem.pdb pvio_npipe.dll pvio_npipe.pdb >NUL 2>NUL

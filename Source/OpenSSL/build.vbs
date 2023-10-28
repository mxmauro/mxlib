Option Explicit
Dim oFso
Dim szPlatform, szPlatformPath, szConfiguration, szConfigurationTarget, szConfigDebug
Dim szScriptPath, szFileName, szDefineNoErr, szIsDebug
Dim szPerlPath, szNasmPath
Dim I, nErr, S, dtBuildDate, bRebuild, aFiles


Set oFso = CreateObject("Scripting.FileSystemObject")

'Check if inside a Visual Studio environment
If CheckVisualStudioCommandPrompt() = False Then
	WScript.Echo "Error: Run this script inside Visual Studio."
	WScript.Quit 1
End If


'Check command-line arguments
If WScript.Arguments.Count = 0 Then
	WScript.Echo "Use: CSCRIPT " & WScript.ScriptName & " /configuration (debug|release) /platform (x86|x64) [/rebuild]"
	WScript.Quit 1
End If

szConfiguration = ""
szPlatform = ""
bRebuild = False

I = 0
Do While I < WScript.Arguments.Count
	S = LCase(WScript.Arguments(I))
	If S = "/?" Or S = "-?" Or S = "/help" Or S = "-help" Then
		WScript.Echo "Use: CSCRIPT " & WScript.ScriptName & _
						" /configuration (debug|release) /platform (x86|x64) [/rebuild]"
		WScript.Quit 1

	ElseIf S = "/configuration" Or S = "-configuration" Then
		If Len(szConfiguration) <> 0 Then
			WScript.Echo "Error: /configuration parameter already specified."
			WScript.Quit 1
		End If

		I = I + 1
		S = ""
		If I < WScript.Arguments.Count Then S = LCase(WScript.Arguments(I))
		If S = "debug" Then
			szConfiguration = "Debug"
			szIsDebug = "debug"
			szConfigDebug = "debug-"
			szDefineNoErr = "--debug"
		ElseIf S = "release" Then
			szConfiguration = "Release"
			szIsDebug = ""
			szConfigDebug = ""
			szDefineNoErr = "no-err"
		Else
			WScript.Echo "Error: Invalid configuration specified."
			WScript.Quit 1
		End If

	ElseIf S = "/platform" Or S = "-platform" Then
		If Len(szPlatform) <> 0 Then
			WScript.Echo "Error: /platform parameter already specified."
			WScript.Quit 1
		End If

		I = I + 1
		S = ""
		If I < WScript.Arguments.Count Then S = LCase(WScript.Arguments(I))

		If S = "x86" Then
			szPlatform = "x86"
			szPlatformPath = "Win32"
			szConfigurationTarget = "my-VC-WIN32"
		ElseIf S = "x64" Then
			szPlatform = "x64"
			szPlatformPath = "x64"
			szConfigurationTarget = "my-VC-WIN64A"
		Else
			WScript.Echo "Error: Invalid platform specified."
			WScript.Quit 1
		End If

	ElseIf S = "/rebuild" Or S = "-rebuild" Then
		If bRebuild <> False Then
			WScript.Echo "Error: /rebuild parameter already specified."
			WScript.Quit 1
		End If
		bRebuild = True

	Else
		WScript.Echo "Error: Invalid parameter."
		WScript.Quit 1
	End If
	I = I + 1
Loop
If Len(szConfiguration) = 0 Then
	WScript.Echo "Error: /configuration parameter not specified."
	WScript.Quit 1
End If
If Len(szPlatform) = 0 Then
	WScript.Echo "Error: /platform parameter not specified."
	WScript.Quit 1
End If


'Setup directories
szScriptPath = Left(WScript.ScriptFullName, Len(WScript.ScriptFullName) - Len(WScript.ScriptName))
szPerlPath = szScriptPath & "..\..\Utilities\Perl5\bin"
szNasmPath = szScriptPath & "..\..\Utilities\Nasm"

If bRebuild = False Then
	WScript.Echo "Checking if source files were modified..."
	S = szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration & "\"
	dtBuildDate = GetLowestFileTimestamp(Array( _
		S & "openssl_libcrypto.lib", _
		S & "openssl_libssl.lib", _
		S & "openssl_libcommon.lib", _
		S & "openssl_libdefault.lib", _
		S & "openssl_liblegacy.lib", _
		S & "openssl_static.pdb" _
	))
	If IsNull(dtBuildDate) Or _
			CheckForNewerFiles(szScriptPath & "Source", dtBuildDate) <> False Or _
			CheckForNewerFile(szScriptPath & "build.vbs", dtBuildDate) <> False Then
		bRebuild = True
	End If
End If

'Rebuild?
If bRebuild = False Then
	WScript.Echo "Libraries are up-to-date"
	WScript.Quit 0
End If

'Start rebuilding
RunApp "MD " & Chr(34) & szScriptPath & "Temp" & Chr(34), "", "", True

WScript.Echo "Creating configuration settings..."
nErr = CreateConfiguration()
If nErr <> 0 Then
	WScript.Echo "Errors found!"
	WScript.Quit nErr
End If

WScript.Echo "Creating makefile..."
S = "perl.exe Configure " & szConfigDebug & szConfigurationTarget & " " & szDefineNoErr
S = S & " no-asm no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared no-srp no-engine no-module"
S = S & " -DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG" ' -DOPENSSL_NO_DEPRECATED -DOPENSSL_API_COMPAT=30000"
S = S & " -DUNICODE -D_UNICODE"
If Len(szIsDebug) = 0 Then S = S & " -DOPENSSL_NO_FILENAMES"
S = S & " " & Chr(34) & "--config=" & szScriptPath & "Temp\compiler_config.conf" & Chr(34) & " "
nErr = RunApp(S, szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
If nErr <> 0 Then
	WScript.Echo "Errors found!"
	WScript.Quit nErr
End If

WScript.Echo "Patching makefile..."
nErr = Patch_Makefile()
If nErr <> 0 Then
	WScript.Echo "Errors found!"
	WScript.Quit nErr
End If

WScript.Echo "Patching rand_win.c..."
nErr = Patch_RAND_WIN_C()
If nErr <> 0 Then
	WScript.Echo "Errors found!"
	WScript.Quit nErr
End If

'Compile
RunApp "NMAKE clean", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, True

WScript.Echo "Compiling..."
nErr = RunApp("NMAKE /S build_libs", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
If nErr <> 0 Then
	WScript.Echo "Errors found!"
	WScript.Quit nErr
End If

'Pause 5 seconds because NMake's processes may still creating the lib WTF?????
WScript.Sleep(5000)

'Move needed files
'RunApp "MD " & Chr(34) & szObjDir & Chr(34), "", "", True

'Copy include files
RunApp "RD /S /Q " & Chr(34) & szScriptPath & "Generated\include\" & szPlatformPath & "\" & szConfiguration & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "Generated" & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "Generated\include" & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "Generated\include\" & szPlatformPath & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "Generated\include\" & szPlatformPath & "\" & szConfiguration & Chr(34), "", "", True
RunApp "XCOPY " & Chr(34) & szScriptPath & "Source\include\*" & Chr(34) & " " & _
                  Chr(34) & szScriptPath & "Generated\include\" & szPlatformPath & "\" & szConfiguration & Chr(34) & _
                  " /E /Q /-Y", "", "", True

'Move library files
RunApp "RD /S /Q " & Chr(34) & szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration & "\OpenSSL" & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "..\..\Libs" & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "..\..\Libs\" & szPlatformPath & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration & Chr(34), "", "", True
RunApp "MD " & Chr(34) & szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration & "\OpenSSL" & Chr(34), "", "", True

aFiles = Array( "openssl_libcrypto.lib", "openssl_libssl.lib", "openssl_libcommon.lib", "openssl_libdefault.lib", "openssl_liblegacy.lib", "openssl_static.pdb" )
For I = LBound(aFiles) To UBound(aFiles)
	RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Source\" & aFiles(I) & Chr(34) & " " & _
	                    Chr(34) & szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration & "\" & aFiles(I) & Chr(34) & "  >NUL 2>NUL", "", "", False
Next

'Clean after compile
RunApp "NMAKE clean", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, True

WScript.Echo "Done!"
WScript.Quit 0


'-------------------------------------------------------------------------------

Function CheckVisualStudioCommandPrompt
Dim oShell

	Set oShell = CreateObject("WScript.Shell")
	If oShell.ExpandEnvironmentStrings("%VCINSTALLDIR%") <> "%VCINSTALLDIR%" Or _
			oShell.ExpandEnvironmentStrings("%VisualStudioVersion%") <> "%VisualStudioVersion%" Or _
			oShell.ExpandEnvironmentStrings("%MSBuildLoadMicrosoftTargetsReadOnly%") <> "%MSBuildLoadMicrosoftTargetsReadOnly%" Then
		CheckVisualStudioCommandPrompt = True
	Else
		CheckVisualStudioCommandPrompt = False
	End If
	Set oShell = Nothing
End Function

Function CheckForNewerFile(szFile, dtBuildDate)
Dim oFile

	CheckForNewerFile = False
	Set oFile = oFso.getFile(szFile)
	If oFile.DateLastModified > dtBuildDate Then
		CheckForNewerFile = True
	End If
	Set oFile = Nothing
End Function

Function CheckForNewerFiles(szFolder, dtBuildDate)
Dim f, oFolder
Dim S, lS

	CheckForNewerFiles = False
	Set oFolder = oFso.GetFolder(szFolder)
	For Each f in oFolder.SubFolders
		If CheckForNewerFiles(szFolder & "\" & f.name, dtBuildDate) <> False Then
			CheckForNewerFiles = True
			Exit Function
		End If
	Next
	For Each f in oFolder.Files
		S = LCase(f.name)
		If Right(S, 4) = ".cpp" Or Right(S, 2) = ".c" Or Right(S, 2) = ".h" Or Right(S, 2) = ".in" Then
			S = szFolder & "\" & f.name
			If CheckForNewerFile(S, dtBuildDate) <> False And oFso.FileExists(S & ".in") = False Then
				WScript.Echo "File: " & Chr(34) & S & Chr(34) & " is newer... rebuilding"
				CheckForNewerFiles = True
				Exit Function
			End If
		End If
	Next
End Function

Function GetLowestFileTimestamp(aFiles)
Dim oFile
Dim I, dtBuildDate

	For I = LBound(aFiles) To UBound(aFiles)
		If oFso.FileExists(aFiles(I)) = False Then
			GetLowestFileTimestamp = Null
			Exit Function
		End If
		Set oFile = oFso.getFile(aFiles(I))
		If I = LBound(aFiles) Then
			dtBuildDate = oFile.DateLastModified
		Else
			If oFile.DateLastModified < dtBuildDate Then dtBuildDate = oFile.DateLastModified
		End If
		Set oFile = Nothing
	Next
	GetLowestFileTimestamp = dtBuildDate
End Function

Function RunApp(szCmdLine, szCurFolder, szEnvPath, bHide)
Dim oFile, oExec, oShell, oShellEnv
Dim I, nRet, szOutputFile

	WScript.Echo "Running: " & szCmdLine
	Set oShell = CreateObject("WScript.Shell")
	On Error Resume Next
	If szCurFolder <> "" Then oShell.CurrentDirectory = szCurFolder
	If szEnvPath <> "" Then
		Set oShellEnv = oShell.Environment("PROCESS")
		oShellEnv("PATH") = szEnvPath & ";" & oShellEnv("PATH")
	End If
	If bHide = False Then
		szOutputFile = oFso.GetSpecialFolder(2)
		If Right(szOutputFile, 1) <> "\" Then szOutputFile = szOutputFile & "\"
		szOutputFile = szOutputFile & oFso.GetTempName
		szCmdLine = "CMD.EXE /S /C " & Chr(34) & szCmdLine & " > " & Chr(34) & szOutputFile & Chr(34) & " 2>&1" & Chr(34)
	Else
		szCmdLine = "CMD.EXE /S /C " & Chr(34) & szCmdLine & Chr(34)
	End If
	nRet = oShell.Run(szCmdLine, 0, True)
	I = Err.Number
	If I <> 0 Then nRet = I
	On Error Goto 0
	If bHide = False Then
		If oFso.FileExists(szOutputFile) <> False Then
			Set oFile = oFso.OpenTextFile(szOutputFile, 1)
			If Not oFile.AtEndOfStream Then WScript.Echo oFile.ReadAll
			oFile.Close
			On Error Resume Next
			oFso.DeleteFile szOutputFile
			On Error Goto 0
			Set oFile = Nothing
		End If
	End If
	RunApp = nRet
End Function

Function CreateConfiguration()
Dim objRE1, objRE2, objRE3, objRE4, objInputFile, objOutputFile
Dim I, S, nArea

	Set objRE1 = New RegExp
	objRE1.Pattern = "^\s*my\s+%targets\s+=\s+\(\s*"
	objRE1.IgnoreCase = True
	objRE1.Global = False

	Set objRE2 = New RegExp
	objRE2.Pattern = "^\s*\);\s*"
	objRE2.IgnoreCase = True
	objRE2.Global = False

	Set objRE3 = New RegExp
	objRE3.Pattern = "^\s*" & Chr(34) & "VC-[^" & Chr(34) & "]*" & Chr(34) & "\s*=>\s*{\s*"
	objRE3.IgnoreCase = True
	objRE3.Global = False

	Set objRE4 = New RegExp
	objRE4.Pattern = "^\s*},\s*"
	objRE4.IgnoreCase = True
	objRE4.Global = False

	On Error Resume Next
	Set objInputFile = oFso.OpenTextFile(szScriptPath & "Source\Configurations\10-main.conf", 1)
	I = Err.Number
	If I <> 0 Then
		CreateConfiguration = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = oFso.CreateTextFile(szScriptPath & "Temp\compiler_config.conf", True)
	I = Err.Number
	If I <> 0 Then
		CreateConfiguration = I
		On Error Goto 0
		Exit Function
	End If

	objOutputFile.Write "my %targets = (" & vbCrLf
	I = Err.Number
	If I <> 0 Then
		CreateConfiguration = I
		On Error Goto 0
		Exit Function
	End If

	nArea = 0
	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine
		I = Err.Number
		If I <> 0 Then
			CreateConfiguration = I
			On Error Goto 0
			Exit Function
		End If

		If nArea = 0 Then
			'start of targets?
			If objRE1.Test(S) <> False Then nArea = 1
		ElseIf nArea = 1 Then
			'end of targets?
			If objRE2.Test(S) <> False Then Exit Do

			'start of a block?
			If objRE3.Test(S) <> False Then
				'get block name
				S = Mid(S, InStr(S, "VC-"))
				S = Left(S, InStr(S, Chr(34)) - 1)

				If S = "VC-noCE-common" Then
					'replace the common block with ours
					objOutputFile.Write "    " & Chr(34) & "base-" & S & Chr(34) & " => {" & vbCrLf & _
					                    "        inherit_from     => [ " & Chr(34) & "base-VC-common" & Chr(34) & " ]," & vbCrLf & _
					                    "        cflags           => add(picker(default => " & Chr(34) & "-DUNICODE -D_UNICODE " & Chr(34) & "," & vbCrLf & _
					                    "                                       debug   => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MTd" & Chr(34) & " : " & Chr(34) & "/MDd" & Chr(34) & ") ." & Chr(34) & " /Od -DDEBUG -D_DEBUG" & Chr(34) & ";" & vbCrLf & _
					                    "                                       }," & vbCrLf & _
					                    "                                       release => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MT" & Chr(34) & " : " & Chr(34) & "/MD" & Chr(34) & ") . " & Chr(34) & " /O2" & Chr(34) & ";" & vbCrLf & _
					                    "                                       }))," & vbCrLf & _
					                    "        lib_cflags       => add(picker(debug   => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MTd" & Chr(34) & " : " & Chr(34) & "/MDd" & Chr(34) & ");" & vbCrLf & _
					                    "                                       }," & vbCrLf & _
					                    "                                       release => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MT" & Chr(34) & " : " & Chr(34) & "/MD" & Chr(34) & ");" & vbCrLf & _
					                    "                                       }))," & vbCrLf & _
					                    "        bin_cflags       => add(picker(debug   => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MTd" & Chr(34) & " : " & Chr(34) & "/MDd" & Chr(34) & ");" & vbCrLf & _
					                    "                                       }," & vbCrLf & _
					                    "                                       release => sub {" & vbCrLf & _
					                    "                                           ($disabled{shared} ? " & Chr(34) & "/MT" & Chr(34) & " : " & Chr(34) & "/MD" & Chr(34) & ");" & vbCrLf & _
					                    "                                       }))," & vbCrLf & _
					                    "        bin_lflags       => add(" & Chr(34) & "/subsystem:console /opt:ref" & Chr(34) & ")," & vbCrLf & _
					                    "        ex_libs          => add(sub {" & vbCrLf & _
					                    "            my @ex_libs = ();" & vbCrLf & _
					                    "            push @ex_libs, 'ws2_32.lib' unless $disabled{sock};" & vbCrLf & _
					                    "            push @ex_libs, 'gdi32.lib advapi32.lib crypt32.lib user32.lib';" & vbCrLf & _
					                    "            return join(" & Chr(34) & " " & Chr(34) & ", @ex_libs);" & vbCrLf & _
					                    "        })," & vbCrLf & _
					                    "    }," & vbCrLf
					I = Err.Number
					If I <> 0 Then
						CreateConfiguration = I
						On Error Goto 0
						Exit Function
					End If

					nArea = 2
				Else
					'not a common block but a VC one
					objOutputFile.Write "    " & Chr(34) & "base-" & S & Chr(34) & " => {" & vbCrLf
					I = Err.Number
					If I <> 0 Then
						CreateConfiguration = I
						On Error Goto 0
						Exit Function
					End If

					nArea = 3
				End If
			End If
		ElseIf nArea = 2 Then
			'skip block content until end
			If objRE4.Test(S) <> False Then nArea = 1
		ElseIf nArea = 3 Then
			'end of block?
			If objRE4.Test(S) <> False Then
				objOutputFile.Write S & vbCrLf
				I = Err.Number
				If I <> 0 Then
					CreateConfiguration = I
					On Error Goto 0
					Exit Function
				End If

				nArea = 1
			Else
				'replace inherited block names
				Do
					I = InStr(S, Chr(34) & "VC-")
					If I > 0 Then S = Left(S, I) & "base-" & Mid(S, I + 1)
				Loop Until I = 0

				objOutputFile.Write S & vbCrLf
				I = Err.Number
				If I <> 0 Then
					CreateConfiguration = I
					On Error Goto 0
					Exit Function
				End If
			End If
		End If
	Loop

	'add our custom targets
	objOutputFile.Write "    " & Chr(34) & "my-VC-WIN32" & Chr(34) & " => {" & vbCrLf & _
	                    "        inherit_from     => [ " & Chr(34) & "base-VC-WIN32" & Chr(34) & " ]," & vbCrLf & _
	                    "        cflags           => add(" & Chr(34) & "-wd4244 -wd4267" & Chr(34) & ")," & vbCrLf & _
	                    "        lflags           => add(" & Chr(34) & "/ignore:4221 LIBCMT.lib" & Chr(34) & ")" & vbCrLf & _
	                    "    }," & vbCrLf & _
	                    "    " & Chr(34) & "my-VC-WIN64A" & Chr(34) & " => {" & vbCrLf & _
	                    "        inherit_from     => [ " & Chr(34) & "base-VC-WIN64A" & Chr(34) & " ]," & vbCrLf & _
	                    "        cflags           => add(" & Chr(34) & "-wd4244 -wd4267" & Chr(34) & ")," & vbCrLf & _
	                    "        lflags           => add(" & Chr(34) & "/ignore:4221 LIBCMT.lib" & Chr(34) & ")" & vbCrLf & _
	                    "    }," & vbCrLf & _
	                    "    " & Chr(34) & "debug-my-VC-WIN32" & Chr(34) & " => {" & vbCrLf & _
	                    "        inherit_from     => [ " & Chr(34) & "base-VC-WIN32" & Chr(34) & " ]," & vbCrLf & _
	                    "        cflags           => add(" & Chr(34) & "-wd4244 -wd4267" & Chr(34) & ")," & vbCrLf & _
	                    "        lflags           => add(" & Chr(34) & "/ignore:4221 LIBCMTD.lib" & Chr(34) & ")" & vbCrLf & _
	                    "    }," & vbCrLf & _
	                    "    " & Chr(34) & "debug-my-VC-WIN64A" & Chr(34) & " => {" & vbCrLf & _
	                    "        inherit_from     => [ " & Chr(34) & "base-VC-WIN64A" & Chr(34) & " ]," & vbCrLf & _
	                    "        cflags           => add(" & Chr(34) & "-wd4244 -wd4267" & Chr(34) & ")," & vbCrLf & _
	                    "        lflags           => add(" & Chr(34) & "/ignore:4221 LIBCMTD.lib" & Chr(34) & ")" & vbCrLf & _
	                    "    }" & vbCrLf & _
	                    ");"

	I = Err.Number
	If I <> 0 Then
		CreateConfiguration = I
		On Error Goto 0
		Exit Function
	End If

	objOutputFile.Close
	objInputFile.Close
	On Error Goto 0
	CreateConfiguration = 0
End Function

Function Patch_RAND_WIN_C()
Dim objRE1, objRE2, objInputFile, objOutputFile
Dim I, S

	Set objRE1 = New RegExp
	objRE1.Pattern = "^\s*#\s*define\s+USE_BCRYPTGENRANDOM\s*"
	objRE1.IgnoreCase = True
	objRE1.Global = False

	Set objRE2 = New RegExp
	objRE2.Pattern = "^\s*#\s*include\s+\" & Chr(34) & "rand_local\.h\" & Chr(34) & "\s*"
	objRE2.IgnoreCase = True
	objRE2.Global = False

	On Error Resume Next
	Set objInputFile = oFso.OpenTextFile(szScriptPath & "Source\providers\implementations\rands\seeding\rand_win.c", 1)
	I = Err.Number
	If I <> 0 Then
		Patch_RAND_WIN_C = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = oFso.CreateTextFile(szScriptPath & "Temp\rand_win.c", True)
	I = Err.Number
	If I <> 0 Then
		Patch_RAND_WIN_C = I
		On Error Goto 0
		Exit Function
	End If
	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine
		I = Err.Number
		If I <> 0 Then
			Patch_RAND_WIN_C = I
			On Error Goto 0
			Exit Function
		End If

		If objRE1.Test(S) <> False Then
			S = ""
		' ElseIf objRE2.Test(S) <> False Then
		' 	S = "#include " & Chr(34) & "..\Source\crypto\rand\rand_local.h" & Chr(34)
		End If

		objOutputFile.Write S & vbCrLf
		I = Err.Number
		If I <> 0 Then
			Patch_RAND_WIN_C = I
			On Error Goto 0
			Exit Function
		End If
	Loop
	objOutputFile.Close
	objInputFile.Close
	On Error Goto 0
	Patch_RAND_WIN_C = 0
End Function

Function Patch_Makefile()
Dim objRE, objInputFile, objOutputFile
Dim I, S, A, Elem, szRandWinPath, Pos

	On Error Resume Next
	Set objInputFile = oFso.OpenTextFile(szScriptPath & "Source\makefile", 1)
	I = Err.Number
	If I <> 0 Then
		Patch_Makefile = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = oFso.CreateTextFile(szScriptPath & "Temp\makefile", True)
	I = Err.Number
	If I <> 0 Then
		Patch_Makefile = I
		On Error Goto 0
		Exit Function
	End If

	szRandWinPath = "providers\implementations\rands\seeding"

	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine
		I = Err.Number
		If I <> 0 Then
			Patch_Makefile = I
			On Error Goto 0
			Exit Function
		End If

		'Replace target libraries locations & names
		S = Replace(S, "libcrypto.lib", "openssl_libcrypto.lib")
		S = Replace(S, "libssl.lib", "openssl_libssl.lib")
		S = Replace(S, "providers\libcommon.lib", "openssl_libcommon.lib")
		S = Replace(S, "providers\libdefault.lib", "openssl_libdefault.lib")
		S = Replace(S, "providers\liblegacy.lib", "openssl_liblegacy.lib")

		'Replace target PDB
		S = Replace(S, "/Fdossl_static.pdb", "/Fdopenssl_static.pdb")

		'Remove app and test libraries from compilation
		If Left(S, 5) = "LIBS=" Then
			A = Split(Mid(S, 6))
			S = "LIBS="
			For Each Elem In A
				If Len(Elem) > 0 And Left(Elem, 5) <> "apps\" And Left(Elem, 5) <> "test\" Then
					S = S & " " & Elem
				End If
			Next
		End If

		'Patch location of rand_win.c
		Pos = InStr(1, S, szRandWinPath & "\rand_win.c", 1)
		If Pos > 0 Then
			S = Left(S, Pos - 1) & "..\Temp" & Mid(S, Pos + Len(szRandWinPath))
		End If

		objOutputFile.Write S & vbCrLf
		I = Err.Number
		If I <> 0 Then
			Patch_Makefile = I
			On Error Goto 0
			Exit Function
		End If
	Loop
	objOutputFile.Close
	objInputFile.Close
	On Error Goto 0

	RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Temp\makefile" & Chr(34) & " " & _
	                    Chr(34) & szScriptPath & "Source\makefile" & Chr(34), "", "", False
	Patch_Makefile = 0
End Function

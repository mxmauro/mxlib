'
' Copyright (C) 2014-2016 Mauro H. Leggieri, Buenos Aires, Argentina.
' All rights reserved.
'

Option Explicit
Dim szPlatform, szPlatformPath, szConfiguration, szConfigurationTarget, szConfigDebug
Dim szScriptPath, szFileName, szDefineNoErr, szIsDebug
Dim szPerlPath, szNasmPath, szObjDir, szLibDir
Dim I, S, dtBuildDate, aTargetFiles, bRebuild
Dim objFS, objShell, oFile


Set objShell = CreateObject("WScript.Shell")
Set objFS = CreateObject("Scripting.FileSystemObject")

'Check if inside a Visual Studio environment
If objShell.ExpandEnvironmentStrings("%VCINSTALLDIR%") = "%VCINSTALLDIR%" And _
        objShell.ExpandEnvironmentStrings("%VisualStudioVersion%") = "%VisualStudioVersion%" And _
        objShell.ExpandEnvironmentStrings("%MSBuildLoadMicrosoftTargetsReadOnly%") = "%MSBuildLoadMicrosoftTargetsReadOnly%" Then
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
		WScript.Echo "Use: CSCRIPT " & WScript.ScriptName & " /configuration (debug|release) /platform (x86|x64) [/rebuild]"
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

'Setup directorie
szScriptPath = Left(Wscript.ScriptFullName, Len(Wscript.ScriptFullName)-Len(Wscript.ScriptName))
szObjDir = szScriptPath & "..\..\obj\" & szPlatformPath & "\" & szConfiguration & "\OpenSSL"
szLibDir = szScriptPath & "..\..\Libs\" & szPlatformPath & "\" & szConfiguration
szPerlPath = szScriptPath & "..\..\Utilities\Perl5\bin"
szNasmPath = szScriptPath & "..\..\Utilities\Nasm"

'Check if we have to rebuild the libraries
aTargetFiles = Array("libssl.lib", "libcrypto.lib", "ossl_static.pdb")
If bRebuild = False Then
	WScript.Echo "Checking if source files were modified..."
	For I = 0 To 2
		szFileName = szLibDir & "\" & aTargetFiles(I)
		If objFS.FileExists(szFileName) = False Then
			WScript.Echo "Library " & Chr(34) & aTargetFiles(I) & Chr(34) & " was not found... rebuilding"
			bRebuild = True
			Exit For
		End If
		Set oFile = objFS.getFile(szFileName)
		If I = 0 Then
			dtBuildDate = oFile.DateLastModified
		Else
			If oFile.DateLastModified < dtBuildDate Then dtBuildDate = oFile.DateLastModified
		End If
	Next
End If

If bRebuild = False Then
	If CheckForNewerFiles(szScriptPath & "Source", dtBuildDate) <> False Or _
	        CheckForNewerFile(szScriptPath & "build.vbs", dtBuildDate) <> False Then
		bRebuild = True
	End If
End If

'Rebuild
If bRebuild <> False Then
	RunApp "MD " & Chr(34) & szScriptPath & "Temp" & Chr(34), "", "", True

	WScript.Echo "Creating configuration settings..."
	I = CreateConfiguration()
	If I = 0 Then
		WScript.Echo "Configuring..."
		S = "perl.exe -d:Confess Configure " & szConfigDebug & szConfigurationTarget & " " & szDefineNoErr & " no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared "
		S = S & "-DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG -DUNICODE -D_UNICODE "
		If Len(szIsDebug) = 0 Then S = S & "-DOPENSSL_NO_FILENAMES "
		S = S & Chr(34) & "--config=" & szScriptPath & "Temp\compiler_config.conf" & Chr(34)
		I = RunApp(S, szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
	End If
	If I = 0 Then
		I = Patch_Makefile()
	End If
	If I = 0 Then
		I = Patch_RAND_WIN_C()
	End If
	If I = 0 Then
		RunApp "NMAKE clean", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, True

		WScript.Echo "Compiling..."
		I = RunApp("NMAKE /S build_generated", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
		If I = 0 Then
			I = RunApp("NMAKE /S build_libs_nodep", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
		End If
		If I = 0 Then
			I = RunApp("NMAKE /S build_engines_nodep", szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
		End If

		If I = 0 Then
			'Pause 5 seconds because NMake's processes may still creating the lib WTF?????
			WScript.Sleep(5000)

			'Move needed files
			RunApp "MD " & Chr(34) & szObjDir & Chr(34), "", "", True
			RunApp "MD " & Chr(34) & szLibDir & Chr(34), "", "", True

			For I = 0 To 2
				RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Source\" & aTargetFiles(I) & Chr(34) & " " & Chr(34) & szLibDir & Chr(34), "", "", False
			Next

			RunApp "MD " & Chr(34) & szScriptPath & "Generated" & Chr(34), "", "", True
			RunApp "MD " & Chr(34) & szScriptPath & "Generated\OpenSSL" & Chr(34), "", "", True

			RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Source\include\openssl\opensslconf.h" & Chr(34) & " " & Chr(34) & szScriptPath & "Generated\OpenSSL" & Chr(34), "", "", False

			'Clean after compile
			RunApp "NMAKE clean", szScriptPath & "Source", "", True

			WScript.Echo "Done!"
			I = 0
		Else
			WScript.Echo "Errors detected while compiling project."
		End If
	Else
		WScript.Echo "Errors detected while preparing files."
	End If
Else
	WScript.Echo "Libraries are up-to-date"
	I = 0
End If
WScript.Quit I

'-------------------------------------------------------------------------------

Function CheckForNewerFile(szFile, dtBuildDate)
Dim oFile

	CheckForNewerFile = False
	Set oFile = objFS.getFile(szFile)
	If oFile.DateLastModified > dtBuildDate Then
		WScript.Echo "File: " & Chr(34) & szFile & Chr(34) & " is newer... rebuilding"
		CheckForNewerFile = True
	End If
End Function

Function CheckForNewerFiles(szFolder, dtBuildDate)
Dim f, oFolder
Dim S, lS

	CheckForNewerFiles = False
	Set oFolder = objFS.GetFolder(szFolder)
	For Each f in oFolder.SubFolders
		If CheckForNewerFiles(szFolder & "\" & f.name, dtBuildDate) <> False Then
			CheckForNewerFiles = True
			Exit Function
		End If
	Next
	For Each f in oFolder.Files
		S = LCase(f.name)
		If Right(S, 4) = ".cpp" Or Right(S, 2) = ".c" Or Right(S, 2) = ".h" Then
			S = szFolder & "\" & f.name
			lS = LCase(S)
			If Right(lS, 33) <> "crypto\include\internal\bn_conf.h" And _
			        Right(lS, 34) <> "crypto\include\internal\dso_conf.h" And _
			        Right(lS, 17) <> "crypto\buildinf.h" And _
			        Right(lS, 29) <> "include\openssl\opensslconf.h" Then
				If CheckForNewerFile(S, dtBuildDate) <> False Then
					CheckForNewerFiles = True
					Exit Function
				End If
			End If
		End If
	Next
End Function

Function RunApp(szCmdLine, szCurFolder, szEnvPath, bHide)
Dim oFso, oFile, oExec, oShell, objShellEnv
Dim I, nRet, szOutputFile

	WScript.Echo "Running: " & szCmdLine
	Set oShell = CreateObject("WScript.Shell")
	On Error Resume Next
	If szCurFolder <> "" Then oShell.CurrentDirectory = szCurFolder
	If szEnvPath <> "" Then
		Set objShellEnv = objShell.Environment("PROCESS")
		objShellEnv("PATH") = szEnvPath & ";" & objShellEnv("PATH")
	End If
	If bHide = False Then
		Set oFso = CreateObject("Scripting.FileSystemObject")
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
	Set objInputFile = objFS.OpenTextFile(szScriptPath & "Source\Configurations\10-main.conf", 1)
	I = Err.Number
	If I <> 0 Then
		CreateConfiguration = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = objFS.CreateTextFile(szScriptPath & "Temp\compiler_config.conf", True)
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
	objRE2.Pattern = "^\s*#\s*include\s+\" & Chr(34) & "rand_lcl\.h\" & Chr(34) & "\s*"
	objRE2.IgnoreCase = True
	objRE2.Global = False

	On Error Resume Next
	Set objInputFile = objFS.OpenTextFile(szScriptPath & "Source\crypto\rand\rand_win.c", 1)
	I = Err.Number
	If I <> 0 Then
		Patch_RAND_WIN_C = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = objFS.CreateTextFile(szScriptPath & "Temp\rand_win.c", True)
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
		ElseIf objRE2.Test(S) <> False Then
			S = "#include " & Chr(34) & "..\Source\crypto\rand\rand_lcl.h" & Chr(34)
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
Dim I, S, Pos

	On Error Resume Next
	Set objInputFile = objFS.OpenTextFile(szScriptPath & "Source\makefile", 1)
	I = Err.Number
	If I <> 0 Then
		Patch_Makefile = I
		On Error Goto 0
		Exit Function
	End If
	Set objOutputFile = objFS.CreateTextFile(szScriptPath & "Temp\makefile", True)
	I = Err.Number
	If I <> 0 Then
		Patch_Makefile = I
		On Error Goto 0
		Exit Function
	End If
	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine
		I = Err.Number
		If I <> 0 Then
			Patch_Makefile = I
			On Error Goto 0
			Exit Function
		End If

		Pos = InStr(1, S, "crypto\rand\rand_win.c", 1)
		If Pos > 0 Then
			S = Left(S, Pos - 1) & "..\Temp" & Mid(S, Pos + 11)
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

	RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Temp\makefile" & Chr(34) & " " & Chr(34) & szScriptPath & "Source\makefile" & Chr(34), "", "", False
	Patch_Makefile = 0
End Function

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
	WScript.Echo "Configuring..."
	S = "perl.exe Configure " & szConfigDebug & szConfigurationTarget & " " & szDefineNoErr & " no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared "
	S = S & "-DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG -DUNICODE -D_UNICODE "
	If Len(szIsDebug) = 0 Then S = S & "-DOPENSSL_NO_FILENAMES "
	S = S & Chr(34) & "--config=" & szScriptPath & "compiler_config.conf" & Chr(34)
	I = RunApp(S, szScriptPath & "Source", szPerlPath & ";" & szNasmPath, False)
	If I = 0 Then
		RunApp "MD " & Chr(34) & szScriptPath & "Temp" & Chr(34), "", "", True

		Call Patch_Makefile()
		Call Patch_RAND_WIN_C()

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
		WScript.Echo "Errors detected while preparing makefiles."
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

Sub Patch_RAND_WIN_C()
Dim objRE1, objRE2, objInputFile, objOutputFile
Dim S

	Set objRE1 = New RegExp
	objRE1.Pattern = "#\s*define\s+USE_BCRYPTGENRANDOM"
	objRE1.IgnoreCase = True
	objRE1.Global = False

	Set objRE2 = New RegExp
	objRE2.Pattern = "#\s*include\s+\" & Chr(34) & "rand_lcl\.h\" & Chr(34)
	objRE2.IgnoreCase = True
	objRE2.Global = False

	Set objInputFile = objFS.OpenTextFile(szScriptPath & "Source\crypto\rand\rand_win.c", 1)
	Set objOutputFile = objFS.CreateTextFile(szScriptPath & "Temp\rand_win.c", True)
	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine

		If objRE1.Test(S) <> False Then
			S = ""
		ElseIf objRE2.Test(S) <> False Then
			S = "#include " & Chr(34) & "..\Source\crypto\rand\rand_lcl.h" & Chr(34)
		End If

		objOutputFile.Write S & vbCrLf
	Loop
	objOutputFile.Close
	objInputFile.Close
End Sub

Sub Patch_Makefile()
Dim objRE, objInputFile, objOutputFile
Dim S, Pos

	Set objInputFile = objFS.OpenTextFile(szScriptPath & "Source\makefile", 1)
	Set objOutputFile = objFS.CreateTextFile(szScriptPath & "Temp\makefile", True)
	Do Until objInputFile.AtEndOfStream
		S = objInputFile.ReadLine

		Pos = InStr(1, S, "crypto\rand\rand_win.c", 1)
		If Pos > 0 Then
			S = Left(S, Pos - 1) & "..\Temp" & Mid(S, Pos + 11)
		End If

		objOutputFile.Write S & vbCrLf
	Loop
	objOutputFile.Close
	objInputFile.Close

	RunApp "MOVE /Y " & Chr(34) & szScriptPath & "Temp\makefile" & Chr(34) & " " & Chr(34) & szScriptPath & "Source\makefile" & Chr(34), "", "", False
End Sub

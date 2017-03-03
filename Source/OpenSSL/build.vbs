'
' Copyright (C) 2014-2016 Mauro H. Leggieri, Buenos Aires, Argentina.
' All rights reserved.
'

Option Explicit
Dim szPlatform, szPlatformPath, szConfiguration, szConfigurationTarget, szConfigDebug
Dim szScriptPath, szFileName, szDefineNoErr, szIsDebug
Dim szPerlPath, szNasmPath, szOrigFolder, szObjDir, szLibDir
Dim I, S, dtBuildDate, aTargetFiles, bRebuild
Dim objFS, objShell, objShellEnv, oFile


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
	If CheckForNewerFiles(szScriptPath & "Source", dtBuildDate) <> False Then bRebuild = True
End If

'Rebuild
If bRebuild <> False Then
	'Setup environment variables
	Set objShellEnv = objShell.Environment("PROCESS")
	objShellEnv("PATH") = szPerlPath & ";" & szNasmPath & ";" & objShellEnv("PATH")

	'Prepare
	szOrigFolder = objShell.CurrentDirectory
	objShell.CurrentDirectory = szScriptPath & "Source"

	WScript.Echo "Configuring..."
	S = "perl.exe Configure " & szConfigDebug & szConfigurationTarget & " " & szDefineNoErr & " no-sock no-rc2 no-idea no-cast no-md2 no-mdc2 no-camellia no-shared "
	S = S & "-DOPENSSL_NO_DGRAM -DOPENSSL_NO_CAPIENG -DOPENSSL_NO_FILENAMES -DUNICODE -D_UNICODE "
	S = S & Chr(34) & "--config=" & szScriptPath & "compiler_config.conf" & Chr(34)
	I = objShell.Run(S, 1, True)
	If I = 0 Then
		objShell.Run "NMAKE clean >NUL 2>NUL", 1, True

		WScript.Echo "Compiling..."
		I = objShell.Run("NMAKE /S build_generated", 1, True)
		If I = 0 Then
			I = objShell.Run("NMAKE /S build_libs_nodep", 1, True)
		End If
		If I = 0 Then
			I = objShell.Run("NMAKE /S build_engines_nodep", 1, True)
		End If

		If I = 0 Then
			'Pause 5 seconds because NMake's processes may still creating the lib WTF?????
			objShell.Run "PING 127.0.0.1 -n 6 >NUL", 1, True

			'Move needed files
			objShell.Run "CMD.EXE /c MD " & Chr(34) & szObjDir & Chr(34) & " >NUL 2>NUL", 0, True
			objShell.Run "CMD.EXE /c MD " & Chr(34) & szLibDir & Chr(34) & " >NUL 2>NUL", 0, True

			For I = 0 To 2
				objShell.Run "CMD.EXE /c MOVE /Y " & Chr(34) & szScriptPath & "Source\" & aTargetFiles(I) & Chr(34) & " " & Chr(34) & szLibDir & Chr(34), 0, True
			Next

			objShell.Run "CMD.EXE /c MD " & Chr(34) & szScriptPath & "Generated\" & Chr(34) & " >NUL 2>NUL", 0, True
			objShell.Run "CMD.EXE /c MD " & Chr(34) & szScriptPath & "Generated\OpenSSL" & Chr(34) & " >NUL 2>NUL", 0, True

			objShell.Run "CMD.EXE /c MOVE /Y " & Chr(34) & szScriptPath & "Source\include\openssl\opensslconf.h" & Chr(34) & " " & Chr(34) & szScriptPath & "Generated\OpenSSL" & Chr(34), 0, True

			'Clean after compile
			objShell.Run "NMAKE clean >NUL 2>NUL", 1, True

			WScript.Echo "Done!"
			I = 0
		Else
			WScript.Echo "Errors detected while compiling project."
		End If
	Else
		WScript.Echo "Errors detected while preparing makefiles."
	End If

	objShell.CurrentDirectory = szOrigFolder
Else
	WScript.Echo "Libraries are up-to-date"
	I = 0
End If
WScript.Quit I

'-------------------------------------------------------------------------------

Function CheckForNewerFiles(szFolder, dtBuildDate)
Dim f, oFolder, oFile
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
			Set oFile = objFS.getFile(S)
			If oFile.DateLastModified > dtBuildDate Then
				lS = LCase(S)
				If Right(lS, 33) <> "crypto\include\internal\bn_conf.h" And _
				        Right(lS, 34) <> "crypto\include\internal\dso_conf.h" And _
				        Right(lS, 17) <> "crypto\buildinf.h" And _
				        Right(lS, 29) <> "include\openssl\opensslconf.h" Then
					WScript.Echo "File: " & Chr(34) & S & Chr(34) & " is newer... rebuilding"
					CheckForNewerFiles = True
					Exit Function
				End If
			End If
		End If
	Next
End Function

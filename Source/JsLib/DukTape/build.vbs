Option Explicit
Dim oFso, oFile
Dim szScriptPath, szFileName, szPythonPath, szOrigFolder
Dim I, nErr, S, dtBuildDate, aTargetFiles, bRebuild


Set oFso = CreateObject("Scripting.FileSystemObject")

'Check if inside a Visual Studio environment
If CheckVisualStudioCommandPrompt() = False Then
	WScript.Echo "Error: Run this script inside Visual Studio."
	WScript.Quit 1
End If


'Check command-line arguments
bRebuild = False

I = 0
Do While I < WScript.Arguments.Count
	S = LCase(WScript.Arguments(I))
	If S = "/?" Or S = "-?" Or S = "/help" Or S = "-help" Then
		WScript.Echo "Use: CSCRIPT " & WScript.ScriptName & " [/rebuild]"
		WScript.Quit 1

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

'Setup directorie
szScriptPath = Left(Wscript.ScriptFullName, Len(Wscript.ScriptFullName) - Len(Wscript.ScriptName))
szPythonPath = szScriptPath & "..\..\..\Utilities\Python27"


'Check if we have to rebuild the libraries
aTargetFiles = Array("Source\dist\duktape.c", "Source\dist\duktape.h", "Source\dist\duk_config.h", _
                     "Source\dist\duk_source_meta.json")
If bRebuild = False Then
	WScript.Echo "Checking if source files were modified..."
	For I = 0 To 3
		szFileName = szScriptPath & "\" & aTargetFiles(I)
		If oFso.FileExists(szFileName) = False Then
			WScript.Echo "Library " & Chr(34) & aTargetFiles(I) & Chr(34) & " was not found... rebuilding"
			bRebuild = True
			Exit For
		End If
		Set oFile = oFso.getFile(szFileName)
		If I = 0 Then
			dtBuildDate = oFile.DateLastModified
		Else
			If oFile.DateLastModified < dtBuildDate Then dtBuildDate = oFile.DateLastModified
		End If
		Set oFile = Nothing
	Next
End If

If bRebuild = False Then
	If CheckForNewerFile(szScriptPath & "duk_custom.h", dtBuildDate) <> False Or _
	        CheckForNewerFile(szScriptPath & "build.vbs", dtBuildDate) <> False Or _
	        CheckForNewerFiles(szScriptPath & "Source\config", dtBuildDate) <> False Or _
	        CheckForNewerFiles(szScriptPath & "Source\src-input", dtBuildDate) <> False Then
		bRebuild = True
	End If
End If

'Rebuild?
If bRebuild = False Then
	WScript.Echo "Distributable files are up-to-date"
	WScript.Quit 0
End If

'Start rebuilding
WScript.Echo "Building..."

RunApp "RD /S /Q " & Chr(34) & szScriptPath & "Source\dist" & Chr(34), szScriptPath & "Source", "", True
RunApp "MD " & Chr(34) & szScriptPath & "Source\dist" & Chr(34), szScriptPath & "Source", "", True

S = Chr(34) & szPythonPath & "\python.exe" & Chr(34) & " " & Chr(34) & szScriptPath & "Source\tools\configure.py" & Chr(34)
S = S & " --fixup-file " & Chr(34) & szScriptPath & "duk_custom.h" & Chr(34) & " --output-directory " & Chr(34) & szScriptPath & "Source\dist" & Chr(34)
nErr = RunApp(S, szScriptPath & "Source", "", False)
If nErr <> 0 Then
	WScript.Echo "Errors detected while building distributable files."
	WScript.Quit nErr
End If

'Pause 5 seconds because Python's processes may still creating the lib WTF?????
WScript.Sleep(5000)

'Fix line endings
WScript.Echo "Fixing line endings..."
nErr = FixEOL(szScriptPath & "Source\dist\duk_config.h")
If nErr = 0 Then
	nErr = FixEOL(szScriptPath & "Source\dist\duktape.h")
End If
If nErr <> 0 Then
	WScript.Echo "Errors found."
	WScript.Quit nErr
End If

'Copy needed files
RunApp "MD " & Chr(34) & szScriptPath & "..\..\..\Include\JsLib\DukTape" & Chr(34), szScriptPath & "Source", "", True
RunApp "COPY /Y " & Chr(34) & szScriptPath & "Source\dist\duk_config.h" & Chr(34) & " " & Chr(34) & szScriptPath & "..\..\..\Include\JsLib\DukTape" & Chr(34), szScriptPath & "Source", "", False
RunApp "COPY /Y " & Chr(34) & szScriptPath & "Source\dist\duktape.h" & Chr(34) & " " & Chr(34) & szScriptPath & "..\..\..\Include\JsLib\DukTape" & Chr(34), szScriptPath & "Source", "", False

WScript.Echo "Done!"
WScript.Quit 0


'-------------------------------------------------------------------------------

Function CheckVisualStudioCommandPrompt
Dim oShell

	Set oShell = CreateObject("WScript.Shell")
	If oShell.ExpandEnvironmentStrings("%VCINSTALLDIR%") <> "%VCINSTALLDIR%" Or _
	    oShell.ExpandEnvironmentStrings("%VisualStudioVersion%") <> "%VisualStudioVersion%" Or _
	    oShell.ExpandEnvironmentStrings("%MSBuildLoadMicrosoftTargetsReadOnly%") <> _
	                                    "%MSBuildLoadMicrosoftTargetsReadOnly%" Then
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
End Function

Function CheckForNewerFiles(szFolder, dtBuildDate)
Dim f, oFolder

	CheckForNewerFiles = False
	Set oFolder = oFso.GetFolder(szFolder)
	For Each f in oFolder.SubFolders
		If CheckForNewerFiles(szFolder & "\" & f.name, dtBuildDate) <> False Then
			CheckForNewerFiles = True
			Exit Function
		End If
	Next
	For Each f in oFolder.Files
		If CheckForNewerFile(szFolder & "\" & f.name, dtBuildDate) <> False Then
			WScript.Echo "File: " & Chr(34) & S & Chr(34) & " is newer... rebuilding"
			CheckForNewerFiles = True
			Exit Function
		End If
	Next
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
	else
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

Function FixEOL(szFileName)
Dim objFile
Dim I, S

	On Error Resume Next
	Set objFile = oFso.OpenTextFile(szFileName, 1)
	I = Err.Number
	If I <> 0 Then
		FixEOL = I
		On Error Goto 0
		Exit Function
	End If
	S = objFile.ReadAll
	I = Err.Number
	If I <> 0 Then
		FixEOL = I
		On Error Goto 0
		Exit Function
	End If
	objFile.Close

	'Fix line endings
	S = Replace(S, Chr(13) & Chr(10), Chr(10)) 'CRLF -> LF
	S = Replace(S, Chr(13), Chr(10))           'CR -> LF
	S = Replace(S, Chr(10), Chr(13) & Chr(10)) 'LF -> CRLF
	If Right(S, 2) <> Chr(13) & Chr(10) Then
		S = S & Chr(13) & Chr(10)
	End If

	Set objFile = oFso.CreateTextFile(szFileName, True)
	I = Err.Number
	If I <> 0 Then
		FixEOL = I
		On Error Goto 0
		Exit Function
	End If
	objFile.Write S
	I = Err.Number
	If I <> 0 Then
		FixEOL = I
		On Error Goto 0
		Exit Function
	End If
	objFile.Close
	On Error Goto 0
	FixEOL = 0
End Function

'
' Copyright (C) 2014-2020 Mauro H. Leggieri, Buenos Aires, Argentina.
' All rights reserved.
'

Option Explicit
Dim oFso
Dim szScriptPath
Dim nErr


Set oFso = CreateObject("Scripting.FileSystemObject")

'Check if inside a Visual Studio environment
If CheckVisualStudioCommandPrompt() = False Then
	WScript.Echo "Error: Run this script inside Visual Studio."
	WScript.Quit 1
End If


'Setup directories
szScriptPath = Left(WScript.ScriptFullName, Len(WScript.ScriptFullName) - Len(WScript.ScriptName))


'Copy files
nErr = CopyFiles("Source\include\rapidjson", "..\..\Include\RapidJSON", "*")
If nErr <> 0 Then
	WScript.Echo "Errors detected while copying files. [" & CStr(nErr) & "]"
	WScript.Quit nErr
End If


'Create the "include-all" file
nErr = CreateIncludeAll(szScriptPath & "..\..\Include\RapidJSON\rapidjson-all.h")
If nErr <> 0 Then
	WScript.Echo "Errors detected while creating the include-all file. [" & CStr(nErr) & "]"
	WScript.Quit nErr
End If


'Done
WScript.Echo "RapidJSON files are up-to-date"
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

Function CopyFiles(szSrcFolder, szDestFolder, szFileMask)
Dim nErr, S

	If Len(szFileMask) > 0 Then szFileMask = " " & szFileMask
	S = "ROBOCOPY.EXE " & Chr(34) & szScriptPath & szSrcFolder & Chr(34) & " " & _
												Chr(34) & szScriptPath & szDestFolder & Chr(34) & " " & _
												szFileMask & " /COPY:DAT /XO /NDL /NJH /NJS /NP /NS /NC /E"
	nErr = RunApp(S, szScriptPath, "", True)
	If nErr < 8 Then nErr = 0
	CopyFiles = nErr
End Function

Function CreateIncludeAll(szFileName)
Dim oFolder, oFile
Dim nErr, S
Dim dtBuildDate

	nErr = 1
	If oFso.FileExists(szFileName) <> False Then
		nErr = 0

		Set oFile = oFso.getFile(szFileName)
		dtBuildDate = oFile.DateLastModified

		Set oFolder = oFso.GetFolder(szScriptPath & "..\..\Include\RapidJSON")
		For Each oFile in oFolder.Files
			If oFile.DateLastModified > dtBuildDate Then
				nErr = 1
				Exit For
			End If
		Next
		Set oFolder = Nothing
	End If
	If nErr = 0 Then
		'File is up-to-date
		CreateIncludeAll = nErr
		Exit Function
	End If

	On Error Resume Next
	Set oFile = oFso.CreateTextFile(szFileName, True)
	nErr = Err.Number
	If nErr <> 0 Then
		CreateIncludeAll = nErr
		Exit Function
	End If

	oFile.WriteLine "#ifndef _RAPIDJSON_INCLUDEALL_H"
	oFile.WriteLine "#define _RAPIDJSON_INCLUDEALL_H"
	oFile.WriteLine ""
	oFile.WriteLine "#include <stdexcept>"
	oFile.WriteLine ""
	oFile.WriteLine "#define RAPIDJSON_ASSERT(x) if (!(x)) throw std::runtime_error(#x)"
	oFile.WriteLine "#define RAPIDJSON_NOEXCEPT_ASSERT(x)"
	oFile.WriteLine ""
	oFile.WriteLine "#include " & Chr(34) & "document.h" & Chr(34)
	oFile.WriteLine "#include " & Chr(34) & "prettywriter.h" & Chr(34)
	oFile.WriteLine "#include " & Chr(34) & "stringbuffer.h" & Chr(34)
	oFile.WriteLine "#include " & Chr(34) & "memorybuffer.h" & Chr(34)
	oFile.WriteLine ""
	oFile.WriteLine "//------------------------------------------------------------------------------"
	oFile.WriteLine ""
	oFile.WriteLine "namespace rapidjson {"
	oFile.WriteLine ""
	oFile.WriteLine "__inline const Value* LookupMember(_In_ const Value &parent, _In_z_ LPCSTR szMemberNameA)"
	oFile.WriteLine "{"
	oFile.WriteLine "  const rapidjson::Value::ConstMemberIterator &member = parent.FindMember(szMemberNameA);"
	oFile.WriteLine "  return (member != parent.MemberEnd()) ? &(member->value) : NULL;"
	oFile.WriteLine "}"
	oFile.WriteLine ""
	oFile.WriteLine "} //namespace rapidjson"
	oFile.WriteLine ""
	oFile.WriteLine "//------------------------------------------------------------------------------"
	oFile.WriteLine ""
	oFile.WriteLine "#endif //_RAPIDJSON_INCLUDEALL_H"

	oFile.Close

	On Error Goto 0

	CreateIncludeAll = 0
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

Function CheckForNewerFile(szFile, dtBuildDate)
Dim oFile

	CheckForNewerFile = False
	Set oFile = oFso.getFile(szFile)
	If oFile.DateLastModified > dtBuildDate Then
		WScript.Echo "File: " & Chr(34) & szFile & Chr(34) & " is newer... rebuilding"
		CheckForNewerFile = True
	End If
End Function

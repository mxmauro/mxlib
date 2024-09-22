'
' Copyright (C) 2014-2016 Mauro H. Leggieri, Buenos Aires, Argentina.
' All rights reserved.
'

Option Explicit
Dim aSrcFiles, szDestFile, szVarName, bDeclareAsStatic, bRebuild
Dim S, S2
Dim I, K, nLen, nArgIdx, nFileIdx, nFilesCount
Dim nFileSize, nTotalFileSizes, nProcessedBytes
Dim objFS, oSrcFile, oDestFile, ts
Dim bDestIsNewer

If WScript.Arguments.Count = 0 Then
	WScript.Echo "Use: CSCRIPT " & WScript.ScriptName & " -i input-file [-i input-file-2 ...] -o output-file -n variable-array-name [options]"
	WScript.Echo "Where: 'options' can be:"
	WScript.Echo "  --static: Include declare variable as static."
	WScript.Echo "  --rebuild: Forces a rebuild of the header."
	WScript.Quit 1
End If

ReDim aSrcFiles(0)
aSrcFiles(0) = ""
nFilesCount = 0
szDestFile = ""
szVarName = ""
bDeclareAsStatic = False
bRebuild = False

'Parse arguments
nArgIdx = 0
Do While nArgIdx < WScript.Arguments.Count
	S = LCase(WScript.Arguments(nArgIdx))
	If S = "-i" Then

		If nArgIdx + 1 >= WScript.Arguments.Count Then
			WScript.Echo "Error: Missing value."
			WScript.Quit 1
		End If

		ReDim Preserve aSrcFiles(nFilesCount + 1)
		aSrcFiles(nFilesCount) = WScript.Arguments(nArgIdx + 1)
		nFilesCount = nFilesCount + 1

		nArgIdx = nArgIdx + 2

	ElseIf S = "-o" Then

		If szDestFile <> "" Then
			WScript.Echo "Error: Output filename already specified."
			WScript.Quit 1
		End If
		If nArgIdx + 1 >= WScript.Arguments.Count Then
			WScript.Echo "Error: Missing value."
			WScript.Quit 1
		End If
	
		szDestFile = WScript.Arguments(nArgIdx + 1)
		nLen = Len(szDestFile)
		If nLen = 0 Then
			WScript.Echo "Error: Invalid output filename name."
			WScript.Quit 1
		End If

		nArgIdx = nArgIdx + 2

	ElseIf S = "-n" Then

		If szVarName <> "" Then
			WScript.Echo "Error: Variable name already specified."
			WScript.Quit 1
		End If
		If nArgIdx + 1 >= WScript.Arguments.Count Then
			WScript.Echo "Error: Missing value."
			WScript.Quit 1
		End If
	
		szVarName = WScript.Arguments(nArgIdx + 1)
		nLen = Len(szVarName)
		If nLen = 0 Then
			WScript.Echo "Error: Invalid variable name."
			WScript.Quit 1
		End If
		For I = 1 To nLen
			K = Asc(Mid(szVarName, I, 1))
			If (K < 65 Or K > 90) And (K < 97 Or K > 122) And (K < 48 Or K > 57 Or I = 1) And K <> 95 Then
				WScript.Echo "Error: Invalid variable name."
				WScript.Quit 1
			End If
		Next

		nArgIdx = nArgIdx + 2

	ElseIf S = "--static" Then

		If bDeclareAsStatic <> False Then
			WScript.Echo "Error: '--static' option already specified."
			WScript.Quit 1
		End If
		bDeclareAsStatic = True
		nArgIdx = nArgIdx + 1

	ElseIf S = "--rebuild" Then

		If bRebuild <> False Then
			WScript.Echo "Error: '--rebuild' option already specified."
			WScript.Quit 1
		End If
		bRebuild = True
		nArgIdx = nArgIdx + 1

	Else

		WScript.Echo "Error: Invalid argument."
		WScript.Quit 1

	End If
Loop
If nFilesCount = 0 Then
	WScript.Echo "Error: No input filename specified."
	WScript.Quit 1
End If
If szDestFile = "" Then
	WScript.Echo "Error: Output filename not specified."
	WScript.Quit 1
End If
If szVarName = "" Then
	WScript.Echo "Error: Variable name not specified."
	WScript.Quit 1
End If

'Begin work
Set objFS = CreateObject("Scripting.FileSystemObject")

'Check if target file exists and get its last modified date
Set oDestFile = Nothing
If bRebuild = False And objFS.FileExists(szDestFile) <> False Then
	Set oDestFile = objFS.getFile(szDestFile)
	bDestIsNewer = True
Else
	bDestIsNewer = False
End If

'Process each source file to check size and date
nTotalFileSizes = 0
For nFileIdx = 0 To nFilesCount - 1
	If objFS.FileExists(aSrcFiles(nFileIdx)) = False Then
		WScript.Echo "Error: File to convert not found."
		WScript.Quit 1
	End If
	Set oSrcFile = objFS.getFile(aSrcFiles(nFileIdx))
	nFileSize = oSrcFile.size
	If nFileSize = 0 Then
		WScript.Echo "Error: Cannot process empty files."
		WScript.Quit 1
	End If

	If bRebuild = False And bDestIsNewer <> False Then
		If oSrcFile.DateLastModified >= oDestFile.DateLastModified Then bDestIsNewer = False
	End If

	nTotalFileSizes = nTotalFileSizes + nFileSize

	Set oSrcFile = Nothing
Next

If bDestIsNewer <> False Then
	WScript.Echo "Header file is up-to-date."
	WScript.Quit 0
End If

'Create destination file
On Error Resume Next
Set oDestFile = objFS.CreateTextFile(szDestFile, True, False)
On Error Goto 0
If IsNull(oDestFile) Then
	WScript.Echo ""
	WScript.Echo "Error: Cannot create output file."
	WScript.Quit 2
End If
'Write header
S = "BYTE " & szVarName & "[] = {" & Chr(13) & Chr(10)
If bDeclareAsStatic <> False Then S = "static " & S
oDestFile.Write S

'Process each source file
S = ""
nProcessedBytes = 0
For nFileIdx = 0 To nFilesCount - 1
	If Len(aSrcFiles(nFileIdx)) < 40 Then
		Wscript.Echo "Processing " & Chr(34) & aSrcFiles(nFileIdx) & Chr(34) & "... "
	Else
		WScript.Echo "Processing " & Chr(34) & "..." & Right(aSrcFiles(nFileIdx), 40) & Chr(34) & "... "
	End If

	Set oSrcFile = objFS.getFile(aSrcFiles(nFileIdx))
	nFileSize = oSrcFile.size
	Set ts = oSrcFile.OpenAsTextStream()

	For I = 0 To nFileSize - 1
		K = Asc(ts.Read(1))  'NOTE: DON'T use AscB

		If (nProcessedBytes And 3) = 0 Then S = S & " "
		S2 = Hex(nProcessedBytes)
		If nProcessedBytes < 65536 Then S2 = Right("0000" & S2, 4)

		S = S & " X_BYTE_ENC(0x" & Right("00" & Hex(K), 2) & ",0x" & S2 & ")"

		If nProcessedBytes < nTotalFileSizes - 1 Then S = S & ","
		If (nProcessedBytes And 3) = 3 Or nProcessedBytes = nTotalFileSizes - 1 Then
			oDestFile.Write S & Chr(13) & Chr(10)
			S = ""
		End If

		nProcessedBytes = nProcessedBytes + 1
	Next

	ts.Close
	Set ts = Nothing
	Set oSrcFile = Nothing
Next

'Write footer
oDestFile.Write "};" & Chr(13) & Chr(10)
oDestFile.Close
Wscript.Echo "Done!"
WScript.Quit 0

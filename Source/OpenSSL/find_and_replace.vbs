Option Explicit
Dim fso, szFilename, szSearch,szReplace, objFile, S

szFilename = WScript.Arguments.Item(0)
szSearch = WScript.Arguments.Item(1)
szReplace = WScript.Arguments.Item(2)

Set fso = CreateObject("Scripting.FileSystemObject")
If fso.FileExists(szFilename) <> False Then
    Set objFile = fso.OpenTextFile(szFilename, 1)
    S = objFile.ReadAll
    S = Replace(S, szSearch, szReplace, 1, -1, 0)
    WScript.StdOut.Write S
End If

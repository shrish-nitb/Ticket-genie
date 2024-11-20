Dim WshShell, filePath

' Check if the argument (file path) is provided
If WScript.Arguments.Count = 0 Then
    WScript.Echo "Usage: cscript print.vbs <filename>"
    WScript.Quit
End If

' Get the file path from the argument
filePath = WScript.Arguments(0)

Set WshShell = CreateObject("WScript.Shell")

' Launch Notepad in hidden mode and print the file
WshShell.Run "notepad /p " & Chr(34) & filePath & Chr(34), 0, True

' Clean up
Set WshShell = Nothing

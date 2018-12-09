#SingleInstance force

ProjectPath = C:\Projects\3DO\DoomPorting\3DOnewDoomPort\

IfWinExist OperaFS (De)Compiler 0.1 [altmer.arts-union.ru]
{
	WinActivate
	Send !{f4} ; Simulates the keypress alt+f4
	Run .\OperaFS\OperaFS.exe
	WinWait, OperaFS[De]Compiler,
	Sleep, 250 ; wait for 200ms
	SEND {Tab}{Space}
	Sleep, 250 ; wait for 200ms
	WinWait, Select Directory,
	SEND {Tab}%ProjectPath%takeme{ENTER}
	Sleep, 250 ; wait for 200ms
	SEND %ProjectPath%optidoom.iso{ENTER}
	Sleep, 250 ; wait for 200ms
	Send !{f4} ; Simulates the keypress alt+f4
}
else
{
	Run .\OperaFS\OperaFS.exe
	WinWait, OperaFS (De)Compiler 0.1 [altmer.arts-union.ru],
	Sleep, 250 ; wait for 200ms
	SEND {Tab}{Space}
	Sleep, 250 ; wait for 200ms
	WinWait, Select Directory,
	SEND {Tab}%ProjectPath%takeme{ENTER}{ENTER}
	Sleep, 250 ; wait for 200ms
	SEND %ProjectPath%optidoom.iso{ENTER}
	Sleep, 250 ; wait for 200ms
	Send !{f4} ; Simulates the keypress alt+f4
}

return

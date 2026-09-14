' SPDX-License-Identifier: MIT
' Run on XP after you make xp-trackpad.sys with the patch program.
Option Explicit
Dim fs, sh, root, app, driver, mode, link
On Error Resume Next
Install
If Err.Number <> 0 Then
    WScript.Echo "Installation failed: " & Err.Description
    WScript.Quit 1
End If
WScript.Quit 0

Sub Install
Set fs = CreateObject("Scripting.FileSystemObject")
Set sh = CreateObject("WScript.Shell")
root = fs.GetParentFolderName(WScript.ScriptFullName)
app = sh.ExpandEnvironmentStrings("%ProgramFiles%") & "\XP Trackpad"
driver = sh.ExpandEnvironmentStrings("%SystemRoot%") & "\System32\drivers\xp-trackpad.sys"
mode = sh.RegRead("HKCU\Software\Apple Inc.\Trackpad\Mode")
If Not fs.FolderExists(app) Then fs.CreateFolder app
fs.CopyFile root & "\bin\trackpad-settings.exe", app & "\trackpad-settings.exe", True
fs.CopyFile root & "\xp-trackpad.sys", driver, True
sh.RegWrite "HKCU\Software\Apple Inc.\Trackpad\Mode", (mode Or 3) And Not 4, "REG_DWORD"
sh.RegWrite "HKCU\Software\Microsoft\Windows\CurrentVersion\Run\XPTrackpad", _
    Chr(34) & app & "\trackpad-settings.exe" & Chr(34) & " --apply-saved", "REG_SZ"
Set link = sh.CreateShortcut(sh.SpecialFolders("Desktop") & "\XP Trackpad Settings.lnk")
link.TargetPath = app & "\trackpad-settings.exe"
link.Save
' Select the driver after the files and settings are in place.
sh.RegWrite "HKLM\SYSTEM\CurrentControlSet\Services\applemtp\ImagePath", "system32\drivers\xp-trackpad.sys", "REG_EXPAND_SZ"
WScript.Echo "Installation is completed. Restart Windows. Then open XP Trackpad Settings on the desktop."
End Sub

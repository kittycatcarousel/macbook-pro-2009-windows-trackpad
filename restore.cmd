@echo off
rem SPDX-License-Identifier: MIT
if not exist "%SystemRoot%\System32\drivers\AppleMTP.sys" exit /b 1
reg add "HKLM\SYSTEM\CurrentControlSet\Services\applemtp" /v ImagePath /t REG_EXPAND_SZ /d "system32\drivers\AppleMTP.sys" /f
if errorlevel 1 exit /b 1
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v XPTrackpad /f >nul 2>&1
echo The original Apple driver is selected. Restart Windows.
exit /b 0

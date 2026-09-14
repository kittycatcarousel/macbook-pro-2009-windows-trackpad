@echo off
rem SPDX-License-Identifier: MIT
setlocal
pushd "%~dp0"
if not exist dist mkdir dist
tar -a -cf dist\xp-trackpad-xp-x86.zip bin\driver-patch.exe bin\trackpad-settings.exe install.vbs restore.cmd README.md LICENSE docs\apple-driver.md docs\driver-interface.md
set "result=%errorlevel%"
popd
exit /b %result%

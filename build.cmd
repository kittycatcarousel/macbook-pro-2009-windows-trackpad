@echo off
rem SPDX-License-Identifier: MIT
setlocal
if defined MSBUILD_EXE_PATH goto build
for /f "delims=" %%I in ('where msbuild.exe 2^>nul') do if not defined MSBUILD_EXE_PATH set "MSBUILD_EXE_PATH=%%I"
if defined MSBUILD_EXE_PATH goto build
set "vswhere=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere%" goto missing
for /f "usebackq delims=" %%I in (`"%vswhere%" -latest -products * -requires Microsoft.VisualStudio.Component.WinXP -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD_EXE_PATH=%%I"
if not defined MSBUILD_EXE_PATH goto missing
:build
"%MSBUILD_EXE_PATH%" "%~dp0build.proj" /nologo /verbosity:minimal %*
exit /b %errorlevel%
:missing
echo Install Visual Studio C++ tools with XP support, or set MSBUILD_EXE_PATH.
exit /b 1

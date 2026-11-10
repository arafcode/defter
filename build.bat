@echo off
setlocal
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto build

set "DEFTER_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%DEFTER_VSWHERE%" goto missing

for /f "usebackq delims=" %%I in (`"%DEFTER_VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "DEFTER_VSROOT=%%I"
if not defined DEFTER_VSROOT goto missing
call "%DEFTER_VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

:build
pushd "%~dp0src" || exit /b 1
if not exist "..\build" mkdir "..\build"
rc /nologo /fo"..\build\defter.res" defter.rc
if errorlevel 1 goto done
cl /nologo /utf-8 /std:c++20 /O1 /GL /Gw /Gy /MT /EHsc /W4 /wd4100 /wd4244 /wd4267 /wd4389 /wd4458 /wd4706 /Fo"..\build\defter.obj" defter.cpp "..\build\defter.res" /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /DYNAMICBASE /NXCOMPAT /MANIFEST:EMBED /MANIFESTINPUT:defter.manifest /OUT:"..\build\Defter.exe" user32.lib gdi32.lib comctl32.lib comdlg32.lib ole32.lib shell32.lib dwmapi.lib uuid.lib

:done
set "DEFTER_RESULT=%errorlevel%"
popd
exit /b %DEFTER_RESULT%

:missing
echo Visual Studio Build Tools, the MSVC x64 toolset, and the Windows SDK are required.
exit /b 1
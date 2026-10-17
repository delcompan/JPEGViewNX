@echo off
setlocal enabledelayedexpansion
set "ROOT=%~dp0"
if not exist "%ROOT%bin\obj" mkdir "%ROOT%bin\obj"

rem ---------------------------------------------------------------- Visual Studio
set "VSPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSPATH=%%i"
)
if not defined VSPATH (
    for %%p in (
        "%ProgramFiles%\Microsoft Visual Studio\2022\Community"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
        "%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional"
        "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
    ) do (
        if exist "%%~p\VC\Auxiliary\Build\vcvars64.bat" set "VSPATH=%%~p"
    )
)
if not defined VSPATH (
    echo [error] Visual Studio with C++ tools not found.
    echo         Install "Desktop development with C++" or run this from a Developer Command Prompt.
    exit /b 1
)
call "%VSPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [error] vcvars64.bat failed.
    exit /b 1
)

rem ---------------------------------------------------------------- icon
if exist "%ROOT%tools\make_icon.ps1" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\make_icon.ps1" %1
    if errorlevel 1 echo [warn] icon generation failed - keeping the existing res\app.ico
)

rem ---------------------------------------------------------------- resources + sources
rc /nologo /fo "%ROOT%bin\obj\app.res" "%ROOT%res\app.rc"
if errorlevel 1 exit /b 1

cl /nologo /utf-8 /EHsc /std:c++17 /W3 /O2 /MT /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE ^
   "%ROOT%src\main.cpp" "%ROOT%src\image.cpp" "%ROOT%src\settings.cpp" "%ROOT%src\formats.cpp" ^
   "%ROOT%src\shellassoc.cpp" "%ROOT%src\assocdlg.cpp" "%ROOT%src\i18n.cpp" ^
   /Fe:"%ROOT%bin\JPEGViewNX.exe" /Fo:"%ROOT%bin\obj\\" ^
   /link /SUBSYSTEM:WINDOWS "%ROOT%bin\obj\app.res" ^
   user32.lib gdi32.lib windowscodecs.lib ole32.lib shell32.lib comdlg32.lib propsys.lib advapi32.lib

if errorlevel 1 exit /b 1
echo.
echo Built: %ROOT%bin\JPEGViewNX.exe
endlocal

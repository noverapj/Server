@echo off
setlocal enabledelayedexpansion

rem ===== Parse arguments =====
set "PROJECT=%~1"
set "CONFIG=%~2"
if not defined CONFIG set "CONFIG=Debug"

if not defined PROJECT (
    echo Usage: build_project.bat ^<project^> [config]
    echo.
    echo Available projects:
    for %%f in ("%~dp0..\build\*.vcxproj") do echo   %%~nf
    exit /b 1
)

rem ===== Find project =====
set "VCXPROJ=%~dp0..\build\%PROJECT%.vcxproj"
if not exist "%VCXPROJ%" (
    echo [ERROR] Project '%PROJECT%' not found
    echo Available projects:
    for %%f in ("%~dp0..\build\*.vcxproj") do echo   %%~nf
    echo.
    echo Run build.bat first to generate project files.
    exit /b 1
)

rem ===== Find MSBuild via vswhere =====
set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] vswhere.exe not found. Install Visual Studio.
    exit /b 1
)
set "MSBUILD="
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo [ERROR] MSBuild not found
    exit /b 1
)

rem ===== Build =====
echo.
echo Project: %PROJECT%
echo Config:  %CONFIG%
echo.

"%MSBUILD%" "%VCXPROJ%" /p:Configuration=%CONFIG% /p:Platform=Win32 /m /v:minimal
if !ERRORLEVEL! equ 0 (
    echo.
    echo [OK] %PROJECT% - %CONFIG% succeeded
    exit /b 0
) else (
    echo.
    echo [FAILED] %PROJECT% - %CONFIG% had errors
    exit /b 1
)

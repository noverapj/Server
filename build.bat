@echo off
setlocal enabledelayedexpansion

rem ===== Find or download premake5.exe =====
set "PREMAKE_URL=https://github.com/premake/premake-core/releases/download/v5.0.0-beta8/premake-5.0.0-beta8-windows.zip"
set "PREMAKE_DIR=%~dp0tool"
set "PREMAKE=%PREMAKE_DIR%\premake5.exe"

if exist "%PREMAKE%" goto :run_premake

where premake5.exe >nul 2>&1 && set "PREMAKE=premake5.exe" && goto :run_premake

echo Downloading premake5 beta8...
if not exist "%PREMAKE_DIR%" mkdir "%PREMAKE_DIR%"
powershell -Command "try { Invoke-WebRequest -Uri '%PREMAKE_URL%' -OutFile '%PREMAKE_DIR%\premake5.zip' -UseBasicParsing } catch { Write-Host 'ERROR: Failed to download premake5' ; exit 1 }"
if not exist "%PREMAKE_DIR%\premake5.zip" (
    echo [ERROR] Failed to download premake5
    exit /b 1
)
powershell -Command "Expand-Archive -Path '%PREMAKE_DIR%\premake5.zip' -DestinationPath '%PREMAKE_DIR%' -Force"
del "%PREMAKE_DIR%\premake5.zip" 2>nul
if not exist "%PREMAKE%" (
    echo [ERROR] premake5.exe not found after extraction
    exit /b 1
)
echo premake5 downloaded to %PREMAKE%

:run_premake
echo Running premake5 vs2010...
"%PREMAKE%" vs2010
if !ERRORLEVEL! neq 0 (
    echo [ERROR] premake5 failed
    exit /b 1
)

echo.
echo [OK] Project files generated in build\
echo Run scripts\build.bat to build.
exit /b 0

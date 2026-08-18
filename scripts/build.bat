@echo off
setlocal enabledelayedexpansion

rem ===== Parse config argument =====
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Debug"

rem ===== All configs =====
set "ALL_CONFIGS=Debug Release"

rem ===== Find solution =====
set "SLN="
for %%f in ("%~dp0..\build\*.sln") do set "SLN=%%f"
if not defined SLN (
    echo [ERROR] No .sln found in build\
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
echo Solution: %SLN%
echo MSBuild:  %MSBUILD%
echo.

set "PASS=0"
set "FAIL=0"
set "FAILED_LIST="

if /i "%CONFIG%"=="All" (
    for %%c in (%ALL_CONFIGS%) do (
        echo ===== Building %%c =====
        "%MSBUILD%" "%SLN%" /p:Configuration=%%c /p:Platform=Win32 /m /v:minimal
        if !ERRORLEVEL! equ 0 (
            set /a PASS+=1
            echo [OK] %%c succeeded
        ) else (
            set /a FAIL+=1
            set "FAILED_LIST=!FAILED_LIST! %%c"
            echo [FAILED] %%c had errors, continuing...
        )
        echo.
    )
) else (
    echo ===== Building %CONFIG% =====
    "%MSBUILD%" "%SLN%" /p:Configuration=%CONFIG% /p:Platform=Win32 /m /v:minimal
    if !ERRORLEVEL! equ 0 (
        set /a PASS+=1
        echo [OK] %CONFIG% succeeded
    ) else (
        set /a FAIL+=1
        set "FAILED_LIST=!FAILED_LIST! %CONFIG%"
        echo [FAILED] %CONFIG% had errors
    )
    echo.
)

rem ===== Summary =====
echo ========================================
echo  BUILD SUMMARY
echo ========================================
echo  Passed:  !PASS!
echo  Failed:  !FAIL!
if !FAIL! gtr 0 echo  Failed configs:!FAILED_LIST!
echo ========================================

if !FAIL! gtr 0 exit /b 1
exit /b 0

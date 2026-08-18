@echo off
setlocal
rem args: <project_src_dir> <output_filename>
set "OUT=%~1\%~2"
set "GITROOT=%~dp0.."
for /f %%i in ('git -C "%GITROOT%" rev-parse --short HEAD 2^>nul') do set "HASH=%%i"
for /f %%i in ('git -C "%GITROOT%" rev-list --count HEAD 2^>nul') do set "REV=%%i"
for /f %%i in ('git -C "%GITROOT%" describe --tags --always --dirty 2^>nul') do set "TAG=%%i"
if not defined HASH set "HASH=nogit"
if not defined REV set "REV=0"
if not defined TAG set "TAG=v0"
for /f "tokens=2 delims==" %%a in ('wmic OS Get localdatetime /value 2^>nul') do set "dt=%%a"
set "TIMESTAMP=%dt:~0,4%/%dt:~4,2%/%dt:~6,2% %dt:~8,2%:%dt:~10,2%:%dt:~12,2%"
>  "%OUT%" echo #pragma once
>> "%OUT%" echo #define FILEVER        %REV%
>> "%OUT%" echo #define STRFILEVER     "%REV%"
>> "%OUT%" echo #define STRINTERNALNAME "%TIMESTAMP%"
>> "%OUT%" echo #define GIT_REV  "%REV%"
>> "%OUT%" echo #define GIT_HASH "%HASH%"
>> "%OUT%" echo #define GIT_TAG  "%TAG%"
>> "%OUT%" echo #define BUILD_DATE __DATE__
endlocal

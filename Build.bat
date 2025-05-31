@echo off
setlocal

set PROJECT_DIR=%~dp0
cd /d "%PROJECT_DIR%"

if "%~1"=="" (
    set BUILD_TYPE=Debug
) else (
    set BUILD_TYPE=%~1
)

set BUILD_DIR=Build
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%" 2>nul
)
cd "%BUILD_DIR%"

cmake -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..
ninja

endlocal
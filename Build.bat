@echo off
setlocal

set PROJECT_DIR=%~dp0
cd /d "%PROJECT_DIR%"

rem Default build type
set BUILD_TYPE=Debug
set DO_CLEAN=0

rem Parse arguments
:parse_args
if "%~1"=="" goto after_args

if /i "%~1"=="clean" (
    set DO_CLEAN=1
) else (
    set BUILD_TYPE=%~1
)
shift
goto parse_args

:after_args

rem Clean if requested
if %DO_CLEAN%==1 (
    echo Cleaning build directory...
    if exist Build (
        rmdir /s /q Build
    )
)

set BUILD_DIR=Build
if not exist "%BUILD_DIR%" (
    mkdir "%BUILD_DIR%" 2>nul
)
cd "%BUILD_DIR%"

cmake -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ..
ninja

endlocal

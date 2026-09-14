@echo off
setlocal enabledelayedexpansion

set BUILD=debug
if /i "%1"=="release" set BUILD=release

set BUILD_DIR=%~dp0Build\Debug
if "!BUILD!"=="release" set BUILD_DIR=%~dp0Build\Release

if exist "!BUILD_DIR!" (
    pushd "!BUILD_DIR!"

    if not exist "Sunbird.exe" (
        echo [Sunbird] Sunbird.exe not found. Run Build.bat %BUILD% first.
        popd
        endlocal
        exit /b 1
    )

    start "" "Sunbird.exe"
    popd
) else (
    echo [Sunbird] Sunbird.exe not found. Run Build.bat %BUILD% first.
    endlocal
    exit /b 1
)

endlocal
exit /b 0

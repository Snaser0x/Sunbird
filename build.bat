@echo off
setlocal enabledelayedexpansion

call vcvarsall.bat x64 >nul 2>&1
if %errorlevel% neq 0 (
	echo [Sunbird] Failed to call vcvarsall.bat. Ensure Microsoft C/C++ build tools are installed and vcvarsall.bat is accessible from the current environment.
	endlocal
	exit /b 1
)

set BUILD=debug
if /i "%1"=="release" set BUILD=release

set BUILD_DIR=%~dp0build\debug
if "%BUILD%"=="release" set BUILD_DIR=%~dp0build\release

if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
pushd "%BUILD_DIR%"

if "%BUILD%"=="debug" (
	echo [Sunbird] Compiling and linking [debug]...
	cl /nologo /std:c++17 /permissive- /MTd /Od /Zi ^
	/I "%~dp0src" ^
	"%~dp0sandbox\main.cpp" ^
	/Fd"Sunbird.pdb" /Fe"Sunbird.exe" ^
	/link /nologo /DEBUG
	if !errorlevel! neq 0 goto error
) else (
	echo [Sunbird] Compiling and linking [release]...
	cl /nologo /std:c++17 /permissive- /MT /O2 ^
	/I "%~dp0src" ^
	"%~dp0sandbox\main.cpp" ^
	/Fe"Sunbird.exe" ^
	/link /nologo
	if !errorlevel! neq 0 goto error
)

echo.
echo [Sunbird] Build succeeded.
popd
endlocal
exit /b 0

:error
echo.
echo [Sunbird] Build failed.
popd
endlocal
exit /b 1

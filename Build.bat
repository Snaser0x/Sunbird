@echo off
setlocal enabledelayedexpansion

call vcvarsall.bat x64 >nul 2>&1
if !errorlevel! neq 0 (
    echo [Sunbird] Failed to call vcvarsall.bat. Ensure Microsoft C/C++ build tools are installed and vcvarsall.bat is accessible from the current environment.
    endlocal
    exit /b 1
)

set BUILD=debug
if /i "%1"=="release" set BUILD=release

set BUILD_DIR=%~dp0Build\Debug
if "!BUILD!"=="release" set BUILD_DIR=%~dp0Build\Release

if not exist "!BUILD_DIR!" mkdir "!BUILD_DIR!"
pushd "!BUILD_DIR!"

if "!BUILD!"=="debug" (
    echo [Sunbird] Compiling and linking [debug]...
    cl /nologo /std:c++20 /permissive- /MTd /Od /Zi /utf-8 ^
    /I "%~dp0Src" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Main.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32StackAllocator.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Window.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Input.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Time.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32File.cpp" ^
    "%~dp0Src\Engine\Renderer\D3D11\D3D11Renderer.cpp" ^
    "%~dp0Src\Engine\Assets\Asset.cpp" ^
    /Fd"Sunbird.pdb" /Fe"Sunbird.exe" ^
    /link /nologo /DEBUG Kernel32.lib User32.lib D3D11.lib DXGI.lib DXGUID.lib
    if !errorlevel! neq 0 goto error

    echo [Sunbird] Compiling and linking cooker [debug]...
    cl /nologo /std:c++20 /permissive- /MTd /Od /Zi /utf-8 ^
    /I "%~dp0Src" /I "%~dp0External" ^
    "%~dp0Src\Cooker\CookerMain.cpp" ^
    "%~dp0Src\Cooker\CookTexture.cpp" ^
    "%~dp0Src\Cooker\CookShader.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32File.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32StackAllocator.cpp" ^
    /Fd"SunbirdCooker.pdb" /Fe"SunbirdCooker.exe" ^
    /link /nologo /DEBUG Kernel32.lib D3DCompiler.lib
    if !errorlevel! neq 0 goto error
) else (
    echo [Sunbird] Compiling and linking [release]...
    cl /nologo /std:c++20 /permissive- /MT /O2 /utf-8^
    /I "%~dp0Src" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Main.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32StackAllocator.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Window.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Input.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32Time.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32File.cpp" ^
    "%~dp0Src\Engine\Renderer\D3D11\D3D11Renderer.cpp" ^
    "%~dp0Src\Engine\Assets\Asset.cpp" ^
    /Fe"Sunbird.exe" ^
    /link /nologo Kernel32.lib User32.lib D3D11.lib DXGI.lib DXGUID.lib
    if !errorlevel! neq 0 goto error

    echo [Sunbird] Compiling and linking cooker [release]...
    cl /nologo /std:c++20 /permissive- /MT /O2 /utf-8^
    /I "%~dp0Src" /I "%~dp0External" ^
    "%~dp0Src\Cooker\CookerMain.cpp" ^
    "%~dp0Src\Cooker\CookTexture.cpp" ^
    "%~dp0Src\Cooker\CookShader.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32File.cpp" ^
    "%~dp0Src\Engine\Platform\Windows\Win32StackAllocator.cpp" ^
    /Fe"SunbirdCooker.exe" ^
    /link /nologo Kernel32.lib D3DCompiler.lib
    if !errorlevel! neq 0 goto error
)

set COOK_FLAGS=
if "!BUILD!"=="debug" set COOK_FLAGS=--debug

echo [Sunbird] Cooking assets [!BUILD!]...
"!BUILD_DIR!\SunbirdCooker.exe" "%~dp0Data" "!BUILD_DIR!\Data" !COOK_FLAGS!
if !errorlevel! neq 0 goto error

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

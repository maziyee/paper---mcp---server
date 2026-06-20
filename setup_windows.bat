@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  MCP Framework - Windows Setup
echo ========================================
echo.

:: ============================================
:: Step 1: Check prerequisites
:: ============================================
echo [1/4] Checking prerequisites...

where git >nul 2>&1
if %errorlevel% neq 0 (
    echo   ERROR: git not found. Please install Git first.
    echo   https://git-scm.com/download/win
    exit /b 1
)
echo   git: OK

where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo   ERROR: cmake not found. Please install CMake first.
    echo   https://cmake.org/download/
    exit /b 1
)
echo   cmake: OK

where python >nul 2>&1
if %errorlevel% neq 0 (
    echo   ERROR: python not found. Please install Python 3 first.
    echo   https://www.python.org/downloads/
    exit /b 1
)
echo   python: OK

:: ============================================
:: Step 2: Setup vcpkg
:: ============================================
echo.
echo [2/4] Setting up vcpkg...

if not defined VCPKG_ROOT (
    if exist "%USERPROFILE%\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=%USERPROFILE%\vcpkg
    ) else if exist "C:\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=C:\vcpkg
    ) else (
        echo   Cloning vcpkg...
        git clone https://github.com/Microsoft/vcpkg.git "%USERPROFILE%\vcpkg"
        if %errorlevel% neq 0 (
            echo   ERROR: Failed to clone vcpkg
            exit /b 1
        )
        call "%USERPROFILE%\vcpkg\bootstrap-vcpkg.bat"
        set VCPKG_ROOT=%USERPROFILE%\vcpkg
    )
)

echo   VCPKG_ROOT=%VCPKG_ROOT%

:: ============================================
:: Step 3: Install C++ dependencies
:: ============================================
echo.
echo [3/4] Installing C++ dependencies...

"%VCPKG_ROOT%\vcpkg" install spdlog nlohmann-json cpp-httplib --triplet x64-mingw-static

:: ============================================
:: Step 4: Build
:: ============================================
echo.
echo [4/4] Building project...

if exist build rmdir /s /q build
mkdir build
cd build

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-mingw-static -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc -static"
if %errorlevel% neq 0 (
    echo   ERROR: CMake configure failed
    exit /b 1
)

cmake --build . --config Release
if %errorlevel% neq 0 (
    echo   ERROR: Build failed
    exit /b 1
)

cd ..

:: ============================================
:: Done
:: ============================================
echo.
echo ========================================
echo  Setup complete!
echo.
echo  Build output: build\Release\test.exe
echo                 build\Release\test_mcp.exe
echo.
echo  Quick test:
echo    build\Release\test_mcp.exe
echo ========================================

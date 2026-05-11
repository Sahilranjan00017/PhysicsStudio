@echo off
REM =============================================================================
REM scripts\package-windows.bat
REM Build the NSIS installer for Physics Simulation Studio on Windows.
REM
REM Prerequisites:
REM   - Visual Studio 2022 (C++ workload) or Build Tools
REM   - Qt 6.8.x (MSVC 2022 x64) — add Qt\6.x\msvc2022_64\bin to PATH
REM   - CMake 3.24+ — add to PATH
REM   - NSIS 3.x (for CPACK_GENERATOR NSIS) — add to PATH
REM   - Ninja (optional, for faster builds)
REM
REM Usage:
REM   scripts\package-windows.bat
REM =============================================================================

setlocal EnableDelayedExpansion

set ROOT=%~dp0..
set BUILD_DIR=%ROOT%\build\windows-msvc-release

echo.
echo ===========================================
echo  Physics Simulation Studio - Win Packager
echo ===========================================

REM ── Configure ──────────────────────────────────────────────────────────────
echo.
echo [1/4] Configuring...
cmake --preset windows-msvc-release -S "%ROOT%"
if errorlevel 1 (
    echo ERROR: CMake configure failed
    exit /b 1
)

REM ── Build ───────────────────────────────────────────────────────────────────
echo.
echo [2/4] Building (Release)...
cmake --build --preset windows-msvc-release --parallel
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

REM ── Test ────────────────────────────────────────────────────────────────────
echo.
echo [3/4] Running tests...
ctest --preset windows-msvc-release --output-on-failure
if errorlevel 1 (
    echo WARNING: Some tests failed - continuing with packaging
)

REM ── Package ─────────────────────────────────────────────────────────────────
echo.
echo [4/4] Creating NSIS installer...
pushd "%BUILD_DIR%"
cpack -G NSIS --verbose
cpack -G ZIP  --verbose
popd
if errorlevel 1 (
    echo ERROR: CPack failed
    exit /b 1
)

REM ── Done ────────────────────────────────────────────────────────────────────
echo.
echo SUCCESS: Installer ready in %BUILD_DIR%
dir "%BUILD_DIR%\PhysicsSimulationStudio-*.exe" 2>nul
dir "%BUILD_DIR%\PhysicsSimulationStudio-*.zip" 2>nul

endlocal

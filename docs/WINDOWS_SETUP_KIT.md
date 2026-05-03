# Windows Setup Kit

This project targets Windows 10/11 with Qt6 and MSVC. The macOS workspace may be used for architecture and portable C++ scaffolding, but Windows validation remains mandatory before Phase 0.2 approval.

## Required Tools

- Windows 10 21H2+ or Windows 11, x64
- Visual Studio Community 2022 with "Desktop development with C++"
- Qt 6.x for MSVC 2022 64-bit
- CMake 3.24+
- Eigen3 headers
- Git 2.x

## Suggested Paths

- Project: `C:\Projects\PhysicsStudio`
- Qt: `C:\Qt\6.8.1\msvc2022_64`
- Eigen: `C:\Libraries\eigen`

## Environment Variables

```bat
set Qt6_DIR=C:\Qt\6.8.1\msvc2022_64
set EIGEN3_INCLUDE_DIR=C:\Libraries\eigen
set PATH=%Qt6_DIR%\bin;%PATH%
```

For persistent setup, add these through Windows System Properties after the first successful command-line validation.

## Validation Commands

Run these in **Developer Command Prompt for VS 2022**:

```bat
cl.exe
cmake --version
qmake --version
dir C:\Libraries\eigen\Eigen
git --version
```

All five must pass before Windows validation can be approved.

## Build Command

```bat
cd C:\Projects\PhysicsStudio
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

If Qt is installed in a non-standard path, pass:

```bat
cmake --preset windows-msvc-release -DQt6_DIR=C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6
```


# Windows Transition Guide

Use this guide when a Windows 10/11 machine or VM becomes available.

## Pre-Windows Checklist

- Windows 10 21H2+ or Windows 11 x64.
- Administrator access.
- 100 GB free disk space.
- Stable internet connection.
- Repository available through GitHub, cloud copy, USB, or shared folder.
- Developer Command Prompt for VS 2022 available after setup.

## Repository Transfer

### Option A: GitHub

On macOS:

```bash
cd "/Users/user/Documents/Window Software/PhysicsStudio"
git remote add origin https://github.com/YOUR_USERNAME/PhysicsStudio.git
git branch -M main
git push -u origin main
```

On Windows:

```bat
cd C:\Projects
git clone https://github.com/YOUR_USERNAME/PhysicsStudio.git
cd PhysicsStudio
git log --oneline
```

Confirm the log includes:

```text
ff5a888 Initial commit: portable Phase 0.1a foundation
```

### Option B: Local Copy

Copy the full `PhysicsStudio` folder, including the hidden `.git` folder, to:

```text
C:\Projects\PhysicsStudio
```

Then run:

```bat
cd C:\Projects\PhysicsStudio
git status
git log --oneline
```

## Tool Setup Order

1. Install Visual Studio Community 2022 with "Desktop development with C++".
2. Install Qt6 for MSVC 2022 64-bit.
3. Install CMake 3.24+ if not already available.
4. Extract Eigen3 to `C:\Libraries\eigen`.
5. Install or verify Git.

## First Commands on Windows

Run inside **Developer Command Prompt for VS 2022**:

```bat
cl.exe
cmake --version
qmake --version
dir C:\Libraries\eigen\Eigen
git --version
```

If `qmake` is not found:

```bat
set PATH=C:\Qt\6.8.1\msvc2022_64\bin;%PATH%
qmake --version
```

## First Build

```bat
cd C:\Projects\PhysicsStudio
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

If CMake cannot find Qt:

```bat
cmake --preset windows-msvc-release -DQt6_DIR=C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6
cmake --build --preset windows-msvc-release
```

## Launch Test

```bat
.\build\windows-msvc-release\Release\PhysicsSimulationStudio.exe
```

If the executable is under a different Visual Studio configuration folder, search:

```bat
dir /s PhysicsSimulationStudio.exe
```

## Troubleshooting

### `cl.exe` not found

Use Developer Command Prompt for VS 2022, not regular Command Prompt or PowerShell.

### `qmake` not found

Add the Qt MSVC bin path to `PATH` for the current shell:

```bat
set PATH=C:\Qt\6.8.1\msvc2022_64\bin;%PATH%
```

### CMake cannot find Qt6

Pass `Qt6_DIR` explicitly:

```bat
-DQt6_DIR=C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6
```

### Build succeeds but EXE fails to start

Run `windeployqt` against the executable for local dependency deployment:

```bat
C:\Qt\6.8.1\msvc2022_64\bin\windeployqt.exe build\windows-msvc-release\Release\PhysicsSimulationStudio.exe
```

### Eigen path missing

Re-extract Eigen so this file exists:

```text
C:\Libraries\eigen\Eigen\Dense
```


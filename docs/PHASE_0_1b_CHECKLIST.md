# Phase 0.1b Windows Validation Checklist

Phase 0.1b starts only when a Windows 10/11 machine or VM is available. Run every command in **Developer Command Prompt for VS 2022**.

## Preconditions

- Repository is available at `C:\Projects\PhysicsStudio`.
- Visual Studio 2022 with MSVC is installed.
- Qt6 MSVC 2022 64-bit is installed.
- CMake 3.24+ is installed or available through Visual Studio.
- Eigen3 is extracted to `C:\Libraries\eigen`.
- Git is installed.

## Test 1: MSVC Compiler

Command:

```bat
cl.exe
```

Expected:

- Microsoft C/C++ compiler version text.
- No `not recognized` error.

Result:

```text
[ ] PASS
[ ] FAIL
```

## Test 2: CMake

Command:

```bat
cmake --version
```

Expected:

- CMake 3.24 or newer.

Result:

```text
[ ] PASS
[ ] FAIL
```

## Test 3: Qt6

Command:

```bat
qmake --version
```

Expected:

- Qt version 6.x.
- Qt path points to an MSVC 2022 64-bit kit.

If needed:

```bat
set PATH=C:\Qt\6.8.1\msvc2022_64\bin;%PATH%
qmake --version
```

Result:

```text
[ ] PASS
[ ] FAIL
```

## Test 4: Eigen3 Headers

Command:

```bat
dir C:\Libraries\eigen\Eigen
```

Expected:

- Header listing includes `Core`, `Dense`, and `Sparse`.

Result:

```text
[ ] PASS
[ ] FAIL
```

## Test 5: Build Project

Command:

```bat
cd C:\Projects\PhysicsStudio
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

Expected:

- CMake configure completes.
- Build completes with 0 errors.
- Target `PhysicsSimulationStudio` is built.

If Qt cannot be found:

```bat
cmake --preset windows-msvc-release -DQt6_DIR=C:\Qt\6.8.1\msvc2022_64\lib\cmake\Qt6
cmake --build --preset windows-msvc-release
```

Result:

```text
[ ] PASS
[ ] FAIL
```

## Test 6: Run Executable

Likely command:

```bat
.\build\windows-msvc-release\Release\PhysicsSimulationStudio.exe
```

If not found:

```bat
dir /s PhysicsSimulationStudio.exe
```

Expected:

- Main window launches.
- Menu bar, toolbar, canvas grid, parts dock, and properties dock are visible.
- Window closes cleanly.

Result:

```text
[ ] PASS
[ ] FAIL
```

## Report Template

Create:

```text
C:\Projects\PhysicsStudio\docs\PHASE_0_1b_WINDOWS_VALIDATION_REPORT.md
```

Template:

````markdown
# Phase 0.1b: Windows Validation Report

**Developer**: [Your Name]
**Date**: [Date]
**Windows Version**: [Windows 10/11 version]
**CPU**: [CPU]
**RAM**: [RAM]
**Status**: COMPLETE

## Test 1: MSVC Compiler

Command: `cl.exe`

Output:
```text
[paste output]
```

Result: [ ] PASS / [ ] FAIL

## Test 2: CMake

Command: `cmake --version`

Output:
```text
[paste output]
```

Result: [ ] PASS / [ ] FAIL

## Test 3: Qt6

Command: `qmake --version`

Output:
```text
[paste output]
```

Result: [ ] PASS / [ ] FAIL

## Test 4: Eigen3

Command: `dir C:\Libraries\eigen\Eigen`

Output:
```text
[paste output]
```

Result: [ ] PASS / [ ] FAIL

## Test 5: Build Project

Command:
`cmake --preset windows-msvc-release`
`cmake --build --preset windows-msvc-release`

Output, last 20 lines:
```text
[paste output]
```

Errors: [0 / list errors]
Result: [ ] PASS / [ ] FAIL

## Test 6: Run Executable

Command: `.\build\windows-msvc-release\Release\PhysicsSimulationStudio.exe`

Result: [ ] Window launches / [ ] Crashes / [ ] Not found
Screenshot: [attach]

## Summary

- Tests passed: [0-6] / 6
- Blockers: [None / list]
- Ready for Phase 0.2: [ ] YES / [ ] NO

## CTO Gate Status

[ ] APPROVED for Phase 0.2
[ ] NEEDS FIXES
````

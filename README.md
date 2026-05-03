# Physics Simulation Studio

Physics Simulation Studio is a Qt6/C++ desktop application for educational physics simulations. The product target is Windows 10/11, with native validation required on a Windows build machine before Phase 0.2 is approved.

## Current Phase

- Phase: 0 - Environment and Architecture
- Active work: portable project foundation on macOS
- Windows gate: deferred until a Windows 10/11 MSVC + Qt6 environment is available

## Planned Domains

- Electronics: Modified Nodal Analysis
- Motion and forces: Newtonian mechanics with RK4 integration
- Optics: ray tracing
- Waves: 1D and 2D finite-difference wave solvers

## Windows Build Target

```bat
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
```

The Windows machine must provide Qt6, MSVC 2022, CMake, Eigen3, and Git before final validation.


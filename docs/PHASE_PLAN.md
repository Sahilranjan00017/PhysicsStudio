# Phase Plan

## Phase 0.1 - Environment and Foundation

Status: in progress on macOS for portable scaffolding; Windows validation deferred.

Deliverables:

- Project directory and Git repo
- CMake/Qt6 application skeleton
- Core module folders
- Starter app shell
- Deferred Windows validation report

Gate:

- Windows 10/11 validation report must pass before Phase 0.2 is approved.

## Phase 0.2 - Project Structure and Build Validation

Start only after Windows validation.

Deliverables:

- Confirm MSVC build
- Confirm Qt window launches
- Confirm Eigen test compiles
- Add first CI/build notes

## Phase 1 - Canvas and Component MVP

Status: implementation complete on macOS; compile/runtime validation pending Windows build gate.

Deliverables:

- [x] Parts panel tree
- [x] Canvas placement
- [x] Selection/delete
- [x] Basic component serialization

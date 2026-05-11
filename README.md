# Physics Simulation Studio

An educational multi-domain physics simulator built with **Qt 6** and **C++20**.
Drag components onto a canvas, wire them up, and watch live simulations across four
independent physics domains — simultaneously.

---

## Features

| Domain | Components | Physics engine |
|---|---|---|
| **Electronics** | 26 types — resistors, caps, inductors, diodes, LEDs, transistors (NPN/PNP), op-amps, logic gates (AND/OR/NOT/NAND/NOR/XOR), oscilloscope, transformer, Zener, potentiometer, fuse, AC/DC sources … | Modified Nodal Analysis (MNA) via Eigen LU; backward-Euler C/L transients; piecewise-linear diode/BJT models |
| **Motion** | Balls, blocks, anchors, springs, ropes, pendulums, ramps, pulleys, wheels, thrusters | Symplectic Euler; impulse collisions; Coulomb friction; quadratic air drag; rolling constraint |
| **Optics** | Light sources, flat mirrors, curved mirrors, lenses, prisms, colour filters, slits, diffraction gratings, beam splitters, polarisers, screens | Geometric ray tracing; Snell refraction; Cauchy dispersion; grating equation; Malus's law |
| **Waves** | Point sources, sound sources, plane waves, barriers/slits, reflective walls, absorbers, ripple pulses, detectors | Analytical 2-D superposition; Huygens principle; image-source reflection; finite-duration envelopes |

### UI
- **Live graph panel** — time-series plots of any component measurement
- **Properties panel** — click any component to inspect and tweak parameters
- **Undo / Redo** — full command-pattern history (Ctrl+Z / Ctrl+Y)
- **Speed control** — 0.25× – 4× real-time multiplier
- **R-key rotation** — rotate selected components 15° per press
- **Fit in View** — Ctrl+0; drag-and-drop placement with grid snapping
- **Save / Open / Save As** — `.pss` JSON project files; unsaved-changes guard
- **8 built-in examples** across all domains

---

## Download

| Platform | Installer |
|---|---|
| macOS 11+ (Universal) | `.dmg` — drag `PhysicsSimulationStudio.app` to Applications |
| Windows 10/11 x64 | `.exe` — NSIS installer with Start-Menu shortcut |
| Linux (Ubuntu 22.04+) | `.tar.gz` or `.deb` |

Grab the latest build from the [**Releases**](../../releases) page.

---

## Building from Source

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| CMake | ≥ 3.24 | [cmake.org](https://cmake.org) |
| Qt 6 | ≥ 6.6 (6.8 recommended) | Widgets module required |
| C++ compiler | C++20 | MSVC 2022, Clang 15+, GCC 12+ |
| Eigen 3 | ≥ 3.4 | Auto-fetched if not found on system |

### Quick start

```bash
# Clone
git clone https://github.com/physicsstudio/app.git
cd app

# macOS / Linux
cmake --preset macos-release          # or linux-release
cmake --build --preset macos-release  --parallel
ctest --preset macos-release

# Windows (Developer Command Prompt)
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

### Available presets

| Preset | Platform | Type |
|---|---|---|
| `macos-release` | macOS | Release, universal binary (arm64+x86_64) |
| `macos-debug` | macOS | Debug |
| `macos-local` | macOS | Debug, Makefile (used for day-to-day dev) |
| `windows-msvc-release` | Windows | Release, VS 2022 |
| `windows-msvc-debug` | Windows | Debug, VS 2022 |
| `linux-release` | Linux | Release, Ninja |
| `linux-debug` | Linux | Debug, Ninja |

### Packaging

```bash
# macOS DMG (universal, ad-hoc signed)
./scripts/package-macos.sh

# macOS DMG (notarisation-ready)
./scripts/package-macos.sh --sign "Developer ID Application: Your Name (TEAM)"

# Windows NSIS installer (run in Developer Command Prompt)
scripts\package-windows.bat

# Any platform — detect OS automatically
./scripts/build-release.sh
```

The packager outputs to `build/<preset>/PhysicsSimulationStudio-<version>-<platform>.*`.

---

## Project layout

```
PhysicsStudio/
├── src/
│   ├── app/               MainWindow — menus, toolbar, docks
│   ├── canvas/            CanvasView, OpticsOverlay, WaveFieldOverlay
│   ├── components/        BaseComponent + domain components
│   │   ├── electronics/   26 ELEC_* types
│   │   ├── motion/        11 MOT_* types
│   │   ├── optics/        11 OPT_* types
│   │   └── wave/          8  WAV_* types
│   ├── interaction/       Command pattern (Add/Move/Delete/Wire)
│   ├── persistence/       ProjectDocument (JSON save/load)
│   ├── simulation/        60 FPS SimulationLoop + 4 domain solvers
│   └── ui/                PartsPanel, PropertiesPanel, GraphPanel
├── resources/             QRC bundle (examples, icons, desktop entry)
├── tests/                 Qt Test unit tests (motion, wave, data-logger)
├── cmake/                 Version.h.in, dmg_setup.scpt, SigningConfig
├── scripts/               package-macos.sh, package-windows.bat, build-release.sh
├── .github/workflows/     ci.yml — GitHub Actions CI/CD
├── CMakeLists.txt
└── CMakePresets.json
```

---

## CI / CD

GitHub Actions runs on every push and pull-request:

| Job | Platforms | Steps |
|---|---|---|
| **build** | macOS 14, Windows 2022, Ubuntu 22.04 | Configure → Build → Test → Package |
| **release** | Ubuntu (publish only) | Create GitHub Release with all platform artifacts |
| **lint** | Ubuntu | clang-tidy static analysis report |

Releases are created automatically when a `v*.*.*` tag is pushed.

---

## Keyboard Shortcuts

| Key | Action |
|---|---|
| **Space** | Play / Pause |
| **F5** | Play |
| **F6** | Pause |
| **R** | Rotate selected component 15° |
| **Ctrl+0** | Fit canvas in view |
| **Ctrl+N** | New model |
| **Ctrl+O** | Open model |
| **Ctrl+S** | Save |
| **Ctrl+Shift+S** | Save As… |
| **Ctrl+Z / Ctrl+Y** | Undo / Redo |
| **Delete** | Delete selected |
| **Ctrl+Scroll** | Zoom in / out |

---

## License

MIT — see [LICENSE.txt](LICENSE.txt).

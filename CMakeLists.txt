cmake_minimum_required(VERSION 3.24)

project(PhysicsSimulationStudio
    VERSION 1.0.3
    DESCRIPTION "Educational multi-domain physics simulation studio"
    HOMEPAGE_URL "https://github.com/physicsstudio/app"
    LANGUAGES CXX
)

# Expose version to the application source.
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/Version.h.in"
    "${CMAKE_BINARY_DIR}/generated/Version.h"
    @ONLY
)

# Optional: Apple Developer identity for code signing.
# Pass -DPSS_CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
# at configure time to enable deep signing on macOS.
set(PSS_CODESIGN_IDENTITY "" CACHE STRING
    "macOS code-signing identity (empty = ad-hoc / no signing)")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)

# ---------------------------------------------------------------------------
# Eigen3 (header-only linear algebra — used by ElectronicsSolver for MNA)
# Try system install first; fall back to FetchContent so Windows builds work
# without any manual setup step.
# ---------------------------------------------------------------------------
find_package(Eigen3 3.4 QUIET)
if(NOT Eigen3_FOUND)
    message(STATUS "Eigen3 not found on system — fetching via FetchContent")
    include(FetchContent)
    FetchContent_Declare(
        Eigen3
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG        3.4.0
        GIT_SHALLOW    TRUE
    )
    set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Eigen3)
endif()

option(PSS_BUILD_TESTS "Build unit tests" ON)

# ---------------------------------------------------------------------------
# macOS OpenGL workaround
# ---------------------------------------------------------------------------
if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
        INTERFACE_LINK_LIBRARIES "-framework OpenGL"
    )
endif()

# ---------------------------------------------------------------------------
# Windows: GUI subsystem + app icon resource
# ---------------------------------------------------------------------------
if(WIN32)
    set(WIN_RESOURCES resources/app.rc)   # generated below if absent
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/resources/app.rc")
        file(WRITE "${CMAKE_SOURCE_DIR}/resources/app.rc"
             "IDI_ICON1 ICON \"app.ico\"\n")
    endif()
else()
    set(WIN_RESOURCES "")
endif()

# ---------------------------------------------------------------------------
# Main application target
# ---------------------------------------------------------------------------
add_executable(PhysicsSimulationStudio WIN32
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/canvas/CanvasView.cpp
    src/canvas/CanvasView.h
    src/canvas/OpticsOverlay.cpp
    src/canvas/OpticsOverlay.h
    src/canvas/WaveFieldOverlay.cpp
    src/canvas/WaveFieldOverlay.h
    src/components/BaseComponent.cpp
    src/components/BaseComponent.h
    src/components/ComponentMimeTypes.h
    src/components/ComponentRegistry.cpp
    src/components/ComponentRegistry.h
    src/components/ConnectionPad.h
    src/components/Wire.cpp
    src/components/Wire.h
    src/components/electronics/ElectronicsComponents.cpp
    src/components/electronics/ElectronicsComponents.h
    src/components/motion/MotionComponents.cpp
    src/components/motion/MotionComponents.h
    src/components/optics/OpticsComponents.cpp
    src/components/optics/OpticsComponents.h
    src/components/wave/WaveComponents.cpp
    src/components/wave/WaveComponents.h
    src/interaction/AddPartCommand.cpp
    src/interaction/AddPartCommand.h
    src/interaction/AddWireCommand.cpp
    src/interaction/AddWireCommand.h
    src/interaction/ChangePropertyCommand.cpp
    src/interaction/ChangePropertyCommand.h
    src/interaction/DeletePartCommand.cpp
    src/interaction/DeletePartCommand.h
    src/interaction/DeleteWireCommand.cpp
    src/interaction/DeleteWireCommand.h
    src/interaction/MovePartCommand.cpp
    src/interaction/MovePartCommand.h
    src/interaction/UndoRedoStack.cpp
    src/interaction/UndoRedoStack.h
    src/simulation/SimulationLoop.cpp
    src/simulation/SimulationLoop.h
    src/simulation/SolverInterfaces.h
    src/simulation/electronics/ElectronicsSolver.cpp
    src/simulation/electronics/ElectronicsSolver.h
    src/simulation/motion/MotionSolver.cpp
    src/simulation/motion/MotionSolver.h
    src/simulation/optics/OpticalSolver.cpp
    src/simulation/optics/OpticalSolver.h
    src/simulation/wave/WaveSolver.cpp
    src/simulation/wave/WaveSolver.h
    src/ui/graph/DataLogger.cpp
    src/ui/graph/DataLogger.h
    src/ui/graph/GraphPanel.cpp
    src/ui/graph/GraphPanel.h
    src/ui/partspanel/PartsPanel.cpp
    src/ui/partspanel/PartsPanel.h
    src/ui/properties/PropertiesPanel.cpp
    src/ui/properties/PropertiesPanel.h
    src/persistence/ProjectDocument.cpp
    src/persistence/ProjectDocument.h
    resources/resources.qrc
    ${WIN_RESOURCES}
)

target_include_directories(PhysicsSimulationStudio PRIVATE
    src
    "${CMAKE_BINARY_DIR}/generated"   # Version.h
)
target_link_libraries(PhysicsSimulationStudio PRIVATE Qt6::Widgets Eigen3::Eigen)

# ---------------------------------------------------------------------------
# Compiler flags
# ---------------------------------------------------------------------------
if(MSVC)
    target_compile_options(PhysicsSimulationStudio PRIVATE /W4 /permissive-)
    # Suppress Windows.h min/max macros and reduce header bloat.
    target_compile_definitions(PhysicsSimulationStudio PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _USE_MATH_DEFINES   # fallback in case any header still uses M_PI
    )
else()
    target_compile_options(PhysicsSimulationStudio PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Windows post-build: bundle Qt DLLs with windeployqt
# ---------------------------------------------------------------------------
if(WIN32)
    find_program(WINDEPLOYQT NAMES windeployqt windeployqt6
        HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET PhysicsSimulationStudio POST_BUILD
            COMMAND "${WINDEPLOYQT}"
                --release
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:PhysicsSimulationStudio>"
            COMMENT "Deploying Qt runtime DLLs..."
            VERBATIM
        )
        message(STATUS "windeployqt found: ${WINDEPLOYQT}")
    else()
        message(WARNING "windeployqt not found — Qt DLLs must be deployed manually")
    endif()
endif()

# ---------------------------------------------------------------------------
# macOS post-build: embed Qt frameworks with macdeployqt, then optionally sign
# ---------------------------------------------------------------------------
if(APPLE)
    find_program(MACDEPLOYQT NAMES macdeployqt macdeployqt6
        HINTS "${Qt6_DIR}/../../../bin")
    if(MACDEPLOYQT)
        add_custom_command(TARGET PhysicsSimulationStudio POST_BUILD
            COMMAND "${MACDEPLOYQT}"
                "$<TARGET_BUNDLE_DIR:PhysicsSimulationStudio>"
                -always-overwrite
                -verbose=1
            COMMENT "Embedding Qt frameworks into .app bundle..."
            VERBATIM
        )
        message(STATUS "macdeployqt found: ${MACDEPLOYQT}")
    else()
        message(WARNING "macdeployqt not found — Qt frameworks not bundled; app may not be redistributable")
    endif()

    # Optional deep code signing (Hardened Runtime required for notarisation).
    if(PSS_CODESIGN_IDENTITY)
        add_custom_command(TARGET PhysicsSimulationStudio POST_BUILD
            COMMAND codesign
                --deep --force --verify --timestamp
                --options runtime
                --sign "${PSS_CODESIGN_IDENTITY}"
                "$<TARGET_BUNDLE_DIR:PhysicsSimulationStudio>"
            COMMENT "Code-signing app bundle with identity: ${PSS_CODESIGN_IDENTITY}"
            VERBATIM
        )
        message(STATUS "Code signing enabled with identity: ${PSS_CODESIGN_IDENTITY}")
    else()
        # Ad-hoc signing so the bundle runs on Apple Silicon without Gatekeeper errors locally.
        add_custom_command(TARGET PhysicsSimulationStudio POST_BUILD
            COMMAND codesign --deep --force --sign "-"
                "$<TARGET_BUNDLE_DIR:PhysicsSimulationStudio>"
            COMMENT "Ad-hoc code signing (local use only)..."
            VERBATIM
        )
    endif()
endif()

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
if(APPLE)
    # Install the whole .app bundle to the root of the package.
    install(TARGETS PhysicsSimulationStudio
        BUNDLE  DESTINATION .
        RUNTIME DESTINATION .
    )
elseif(WIN32)
    # Install the EXE and everything windeployqt placed alongside it.
    install(TARGETS PhysicsSimulationStudio RUNTIME DESTINATION .)
    install(DIRECTORY "$<TARGET_FILE_DIR:PhysicsSimulationStudio>/"
        DESTINATION .
        FILES_MATCHING
            PATTERN "*.dll"
            PATTERN "*.exe"
            PATTERN "plugins/*"
    )
    # Bundle VC++ 2022 redist if it was downloaded during CI build
    if(EXISTS "${CMAKE_BINARY_DIR}/vc_redist.x64.exe")
        install(FILES "${CMAKE_BINARY_DIR}/vc_redist.x64.exe" DESTINATION .)
    endif()
else()
    # Linux: binary → /usr/local/bin, desktop entry → share/applications.
    install(TARGETS PhysicsSimulationStudio RUNTIME DESTINATION bin)
    install(FILES "${CMAKE_SOURCE_DIR}/resources/physicsstudio.desktop"
        DESTINATION "share/applications"
        OPTIONAL
    )
endif()

# macOS app bundle properties.
if(APPLE)
    set_target_properties(PhysicsSimulationStudio PROPERTIES
        # Set install rpath to the bundle-relative Frameworks directory.
        # macdeployqt copies Qt frameworks here and rewrites all library
        # references to use @rpath/... notation.  By setting this at build
        # time the binary never carries the Qt install-directory rpath, so
        # CPack's post-install BundleUtilities fixup finds nothing to delete
        # and the "no LC_RPATH load command" error is avoided.
        INSTALL_RPATH              "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH   FALSE
        MACOSX_BUNDLE                 TRUE
        MACOSX_BUNDLE_BUNDLE_NAME     "Physics Simulation Studio"
        MACOSX_BUNDLE_BUNDLE_VERSION  "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_GUI_IDENTIFIER  "com.physicsstudio.app"
        MACOSX_BUNDLE_COPYRIGHT       "Copyright © 2026 PhysicsStudio"
        MACOSX_BUNDLE_INFO_STRING     "Educational physics simulation studio v${PROJECT_VERSION}"
    )
endif()

# ---------------------------------------------------------------------------
# CPack — cross-platform installer/package generation
#
# macOS  :  cmake --build build/macos-release  --target package
#           → build/macos-release/PhysicsSimulationStudio-1.0.0-Darwin.dmg
#
# Windows:  cmake --build build/windows-msvc-release  --target package
#           → build/windows-msvc-release/PhysicsSimulationStudio-1.0.0-win64.exe
#
# Linux  :  cmake --build build/linux-release  --target package
#           → build/linux-release/PhysicsSimulationStudio-1.0.0-Linux.tar.gz
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME                 "PhysicsSimulationStudio")
set(CPACK_PACKAGE_VENDOR               "PhysicsStudio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY  "Educational multi-domain physics simulator")
set(CPACK_PACKAGE_DESCRIPTION_FILE     "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VERSION_MAJOR        "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR        "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH        "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY    "Physics Simulation Studio")
set(CPACK_RESOURCE_FILE_LICENSE        "${CMAKE_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
    # ── Windows: NSIS installer + ZIP portable ──────────────────────────────
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_PACKAGE_FILE_NAME
        "PhysicsSimulationStudio-${PROJECT_VERSION}-win64")
    set(CPACK_NSIS_DISPLAY_NAME     "Physics Simulation Studio ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME     "Physics Simulation Studio")
    set(CPACK_NSIS_HELP_LINK        "https://github.com/physicsstudio/app")
    set(CPACK_NSIS_URL_INFO_ABOUT   "https://github.com/physicsstudio/app")
    set(CPACK_NSIS_CONTACT          "support@physicsstudio.app")
    set(CPACK_NSIS_MODIFY_PATH      ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON         "${CMAKE_SOURCE_DIR}/resources/app.ico")
    set(CPACK_NSIS_MUI_UNIICON      "${CMAKE_SOURCE_DIR}/resources/app.ico")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortcut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Physics Studio.lnk'\
 '$INSTDIR\\\\PhysicsSimulationStudio.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Physics Studio.lnk'")
    # Install VC++ 2022 Redistributable silently so the app always starts
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "ExecWait '\\\"$INSTDIR\\\\vc_redist.x64.exe\\\" /install /quiet /norestart'")

elseif(APPLE)
    # ── macOS: Drag-and-drop DMG ─────────────────────────────────────────────
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_FILE_NAME
        "PhysicsSimulationStudio-${PROJECT_VERSION}-macOS")
    set(CPACK_DMG_VOLUME_NAME       "Physics Simulation Studio ${PROJECT_VERSION}")
    set(CPACK_DMG_FORMAT            "UDZO")   # compressed
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT
        "${CMAKE_SOURCE_DIR}/cmake/dmg_setup.scpt")
    # Optional designer background (skip gracefully if absent).
    if(EXISTS "${CMAKE_SOURCE_DIR}/resources/dmg_background.png")
        set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/resources/dmg_background.png")
    endif()

else()
    # ── Linux: TGZ + DEB ─────────────────────────────────────────────────────
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_PACKAGE_FILE_NAME
        "PhysicsSimulationStudio-${PROJECT_VERSION}-Linux-x64")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER  "PhysicsStudio <support@physicsstudio.app>")
    set(CPACK_DEBIAN_PACKAGE_SECTION     "education")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY    "optional")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS
        "libqt6widgets6 (>= 6.4), libqt6opengl6 (>= 6.4), libstdc++6 (>= 11)")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION
        "Physics Simulation Studio\n"
        " An educational multi-domain physics simulator.\n"
        " Supports Electronics (MNA), Motion (Newtonian), Optics (ray tracing)\n"
        " and Waves (2-D superposition).")
endif()

include(CPack)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(PSS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

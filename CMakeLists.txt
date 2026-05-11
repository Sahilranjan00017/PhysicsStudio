cmake_minimum_required(VERSION 3.24)

project(PhysicsSimulationStudio
    VERSION 1.0.0
    DESCRIPTION "Educational physics simulation studio"
    LANGUAGES CXX
)

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

target_include_directories(PhysicsSimulationStudio PRIVATE src)
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
# Install rules
# ---------------------------------------------------------------------------
install(TARGETS PhysicsSimulationStudio RUNTIME DESTINATION .)

# macOS app bundle
if(APPLE)
    set_target_properties(PhysicsSimulationStudio PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Physics Simulation Studio"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.physicsstudio.app"
    )
endif()

# ---------------------------------------------------------------------------
# CPack — installer generation
# Run: cmake --build build && cpack --config build/CPackConfig.cmake
# ---------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME                "Physics Simulation Studio")
set(CPACK_PACKAGE_VENDOR              "PhysicsStudio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Educational multi-domain physics simulator")
set(CPACK_PACKAGE_VERSION_MAJOR       ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR       ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH       ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_INSTALL_DIRECTORY   "Physics Simulation Studio")
set(CPACK_RESOURCE_FILE_LICENSE       "${CMAKE_SOURCE_DIR}/LICENSE.txt")

if(WIN32)
    # NSIS installer (install NSIS on the build machine; no extra CMake deps)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME   "Physics Simulation Studio")
    set(CPACK_NSIS_PACKAGE_NAME   "Physics Simulation Studio")
    set(CPACK_NSIS_HELP_LINK      "https://github.com/physicsstudio/app")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/physicsstudio/app")
    set(CPACK_NSIS_CONTACT        "support@physicsstudio.app")
    set(CPACK_NSIS_MODIFY_PATH    ON)
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortcut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Physics Studio.lnk' \
'$INSTDIR\\\\PhysicsSimulationStudio.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Physics Studio.lnk'")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Physics Simulation Studio")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/resources/dmg_background.png")
else()
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "PhysicsStudio")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6, libstdc++6")
endif()

include(CPack)

# ---------------------------------------------------------------------------
# Unit tests
# ---------------------------------------------------------------------------
if(PSS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

cmake_minimum_required(VERSION 3.24)

project(PhysicsSimulationStudio
    VERSION 0.1.0
    DESCRIPTION "Educational physics simulation studio for Windows desktop"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

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

if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
    set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES
        INTERFACE_LINK_LIBRARIES "-framework OpenGL"
    )
endif()

add_executable(PhysicsSimulationStudio
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
)

target_include_directories(PhysicsSimulationStudio PRIVATE src)
target_link_libraries(PhysicsSimulationStudio PRIVATE Qt6::Widgets Eigen3::Eigen)

if(MSVC)
    target_compile_options(PhysicsSimulationStudio PRIVATE /W4 /permissive-)
else()
    target_compile_options(PhysicsSimulationStudio PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS PhysicsSimulationStudio RUNTIME DESTINATION .)

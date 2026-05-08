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

option(PSS_BUILD_TESTS "Build unit tests" ON)

add_executable(PhysicsSimulationStudio
    src/main.cpp
    src/app/MainWindow.cpp
    src/app/MainWindow.h
    src/canvas/CanvasView.cpp
    src/canvas/CanvasView.h
    src/components/BaseComponent.cpp
    src/components/BaseComponent.h
    src/components/ComponentRegistry.cpp
    src/components/ComponentRegistry.h
    src/components/ConnectionPad.h
    src/components/Wire.cpp
    src/components/Wire.h
    src/components/electronics/ElectronicsComponents.cpp
    src/components/electronics/ElectronicsComponents.h
    src/interaction/UndoRedoStack.cpp
    src/interaction/UndoRedoStack.h
    src/simulation/SimulationLoop.cpp
    src/simulation/SimulationLoop.h
    src/simulation/SolverInterfaces.h
    src/simulation/electronics/ElectronicsSolver.cpp
    src/simulation/electronics/ElectronicsSolver.h
    src/ui/partspanel/PartsPanel.cpp
    src/ui/partspanel/PartsPanel.h
    src/persistence/ProjectDocument.cpp
    src/persistence/ProjectDocument.h
)

target_include_directories(PhysicsSimulationStudio PRIVATE src)
target_link_libraries(PhysicsSimulationStudio PRIVATE Qt6::Widgets)

if(MSVC)
    target_compile_options(PhysicsSimulationStudio PRIVATE /W4 /permissive-)
else()
    target_compile_options(PhysicsSimulationStudio PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS PhysicsSimulationStudio RUNTIME DESTINATION .)

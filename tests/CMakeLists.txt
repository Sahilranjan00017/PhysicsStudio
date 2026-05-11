# ---------------------------------------------------------------------------
# Unit tests for Physics Simulation Studio
# Run with: cmake --build build && ctest --test-dir build --output-on-failure
# ---------------------------------------------------------------------------

# ── Motion Solver ───────────────────────────────────────────────────────────
# Tests pure physics math (gravity, integration, boundary, collision).
# MotionBody / MotionDomain are plain structs; no QApplication needed.
add_executable(test_motion_solver
    test_motion_solver.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/motion/MotionSolver.cpp
)
target_include_directories(test_motion_solver PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_motion_solver PRIVATE Qt6::Test Qt6::Widgets Eigen3::Eigen)
if(MSVC)
    target_compile_definitions(test_motion_solver PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_motion_solver PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_motion_solver PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME MotionSolverTest COMMAND test_motion_solver)

# ── Wave Solver ─────────────────────────────────────────────────────────────
# Tests analytical superposition: single-source field, linearity, detector.
# Requires QApplication (QPointF scene coordinates used inside WaveSolver).
add_executable(test_wave_solver
    test_wave_solver.cpp
    ${CMAKE_SOURCE_DIR}/src/simulation/wave/WaveSolver.cpp
    ${CMAKE_SOURCE_DIR}/src/components/BaseComponent.cpp
    ${CMAKE_SOURCE_DIR}/src/components/Wire.cpp
    # WaveComponents.cpp intentionally excluded: the test uses inline stubs
    # so it doesn't pull in the ComponentRegistry dependency.
)
target_include_directories(test_wave_solver PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_wave_solver PRIVATE Qt6::Test Qt6::Widgets Eigen3::Eigen)
if(MSVC)
    target_compile_definitions(test_wave_solver PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _USE_MATH_DEFINES)
    target_compile_options(test_wave_solver PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_wave_solver PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME WaveSolverTest COMMAND test_wave_solver)

# ── Data Logger ─────────────────────────────────────────────────────────────
add_executable(test_data_logger
    test_data_logger.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/graph/DataLogger.cpp
    ${CMAKE_SOURCE_DIR}/src/components/BaseComponent.cpp
    ${CMAKE_SOURCE_DIR}/src/components/Wire.cpp
)
target_include_directories(test_data_logger PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_data_logger PRIVATE Qt6::Test Qt6::Widgets Eigen3::Eigen)
if(MSVC)
    target_compile_definitions(test_data_logger PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_data_logger PRIVATE /W4 /permissive-)
else()
    target_compile_options(test_data_logger PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_test(NAME DataLoggerTest COMMAND test_data_logger)

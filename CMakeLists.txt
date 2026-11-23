cmake_minimum_required(VERSION 3.24)
project(Engine LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
option(ENGINE_BUILD_GAME "Build the Windows/DirectX application" ${WIN32})

add_library(game_api INTERFACE)
target_include_directories(game_api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
file(GLOB_RECURSE SIMULATION_SOURCES CONFIGURE_DEPENDS "src/simulation/*.cpp")
add_library(simulation STATIC ${SIMULATION_SOURCES})
target_link_libraries(simulation PUBLIC game_api)

add_custom_target(check_boundaries ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_boundaries.py"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" VERBATIM)
add_dependencies(simulation check_boundaries)

if(ENGINE_BUILD_GAME)
    if(NOT WIN32)
        message(FATAL_ERROR "The application backend requires Windows; simulation/tests do not")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/engine/graphics/shaders/cube.hlsl")
    file(READ "src/engine/graphics/shaders/cube.hlsl" CUBE_SHADER_SOURCE)
    configure_file(src/engine/graphics/shaders/cube_shader.h.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/cube_shader.h" @ONLY)
    file(GLOB_RECURSE RUNTIME_SOURCES CONFIGURE_DEPENDS "src/engine/*.cpp")
    add_library(runtime STATIC ${RUNTIME_SOURCES})
    target_link_libraries(runtime PUBLIC game_api PRIVATE simulation
        d3d12 dxgi d3dcompiler windowscodecs ole32 comctl32 xaudio2_9)
    target_include_directories(runtime PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/external" "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXMath/Inc")
    add_executable(Engine src/game/game.cpp src/game/terrain.cpp)
    target_link_libraries(Engine PRIVATE runtime)
    if(MINGW)
        target_link_options(Engine PRIVATE -static-libgcc -static-libstdc++)
    endif()
    add_custom_target(stage_assets
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/stage_assets.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/minecraft" "$<TARGET_FILE_DIR:Engine>/assets/minecraft"
        VERBATIM)
    add_dependencies(Engine stage_assets check_boundaries)
endif()

if(BUILD_TESTING)
    add_executable(simulation_tests tests/simulation_tests.cpp src/engine/runtime/ticker.cpp)
    target_link_libraries(simulation_tests PRIVATE simulation)
    add_test(NAME simulation COMMAND simulation_tests)
    if(WIN32)
        add_executable(mesh_tests tests/mesh_tests.cpp)
        target_link_libraries(mesh_tests PRIVATE simulation)
        target_include_directories(mesh_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXMath/Inc")
        add_test(NAME terrain_mesh COMMAND mesh_tests)
        set_tests_properties(terrain_mesh PROPERTIES TIMEOUT 30)
    endif()
    add_test(NAME architecture COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/check_boundaries.py")
    if(MINGW)
        target_link_options(simulation_tests PRIVATE -static-libgcc -static-libstdc++)
        if(TARGET mesh_tests)
            target_link_options(mesh_tests PRIVATE -static-libgcc -static-libstdc++)
        endif()
    endif()
    set_tests_properties(simulation architecture PROPERTIES TIMEOUT 30)
endif()
if(MINGW)
    get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    find_file(MINGW_PTHREAD_RUNTIME libwinpthread-1.dll PATHS "${MINGW_BIN}" NO_DEFAULT_PATH REQUIRED)
    foreach(executable Engine simulation_tests mesh_tests)
        if(TARGET ${executable})
            add_custom_command(TARGET ${executable} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MINGW_PTHREAD_RUNTIME}"
                    "$<TARGET_FILE_DIR:${executable}>" VERBATIM)
        endif()
    endforeach()
endif()
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source" FILES ${SIMULATION_SOURCES} ${RUNTIME_SOURCES})

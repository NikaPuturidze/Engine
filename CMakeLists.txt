cmake_minimum_required(VERSION 4.4)
project(Engine)

include(CTest)

set(CMAKE_CXX_STANDARD 26)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/cube.hlsl")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/graphics/shaders/cube.hlsl" CUBE_SHADER_SOURCE)

configure_file(
        src/graphics/shaders/cube_shader.h.in
        "${CMAKE_CURRENT_BINARY_DIR}/generated/cube_shader.h"
        @ONLY
)

add_executable(Engine
        src/main.cpp
        src/core/window.cpp
        src/core/window.h
        src/world/block_registry.cpp
        src/world/block_registry.h
        src/graphics/renderer.cpp
        src/graphics/renderer.h
        src/ui/overlay.cpp
        src/ui/overlay.h
        src/ui/ui_element.h
        src/ui/crosshair.cpp
        src/ui/crosshair.h
        src/ui/target_overlay.h
        src/world/block_raycast.h
        src/ui/debug_overlay.cpp
        src/ui/debug_overlay.h
        src/graphics/cube.h
		src/graphics/texture.h
		src/graphics/texture.cpp
        src/graphics/meshes/cube_mesh.h
        src/graphics/meshes/terrain_mesh.h
        src/graphics/shaders/cube.hlsl
        src/core/camera/Camera.cpp
        src/core/camera/Camera.h
        src/core/player/player.h
		src/core/player/player.cpp
        src/core/player/player_controller.h
        src/core/player/player_controller.cpp
		src/core/engine/engine.cpp
		src/core/engine/engine.h
		src/core/engine/ticker.cpp
		src/core/engine/ticker.h
		src/world/generator/chunk.h
		src/world/world.h
		src/world/world.cpp
		src/world/generator/world_generator.h
		src/world/generator/flat_world_generator.h
		src/world/generator/flat_world_generator.cpp
		src/core/entity/entity.cpp
		src/core/entity/entity.h
)

target_include_directories(Engine PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
        "${CMAKE_CURRENT_SOURCE_DIR}/external"
        "${CMAKE_CURRENT_SOURCE_DIR}/external/DirectXMath/Inc"
)

target_compile_definitions(Engine PRIVATE
		ENGINE_ASSET_DIRECTORY="assets"
)

target_link_libraries(Engine PRIVATE
        d3d12
        dxgi
        d3dcompiler
		windowscodecs
		ole32
		comctl32
)

target_link_options(Engine PRIVATE
				-static-libgcc
		-static-libstdc++
)

add_custom_command(TARGET Engine POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${CMAKE_CURRENT_SOURCE_DIR}/assets"
		"$<TARGET_FILE_DIR:Engine>/assets"
)

if(BUILD_TESTING)
    add_executable(FrustumTests tests/frustum_tests.cpp src/core/camera/camera.cpp
        src/world/world.cpp src/core/player/player.cpp src/core/entity/entity.cpp)
    target_include_directories(FrustumTests PRIVATE src external/DirectXMath/Inc)
    target_link_options(FrustumTests PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME FrustumTests COMMAND FrustumTests)
endif()

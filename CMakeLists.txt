cmake_minimum_required(VERSION 3.16)
project(3DRenderer
    VERSION 3.0
    DESCRIPTION "Retro-terminal 3D renderer with SDL2"
    LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Compile settings
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# Dependencies — prefer a system SDL2, fall back to ./external/SDL2-*
# ---------------------------------------------------------------------------
find_package(SDL2 QUIET)

if(NOT SDL2_FOUND)
    file(GLOB SDL2_LOCAL_DIRS "${CMAKE_SOURCE_DIR}/external/SDL2-*")
    foreach(_dir IN LISTS SDL2_LOCAL_DIRS)
        if(EXISTS "${_dir}/x86_64-w64-mingw32/include/SDL2")
            set(SDL2_INCLUDE_DIRS "${_dir}/x86_64-w64-mingw32/include/SDL2")
            set(SDL2_LIB_DIR      "${_dir}/x86_64-w64-mingw32/lib")
            set(SDL2_LIBRARIES    mingw32 SDL2main SDL2)
            set(SDL2_FOUND TRUE)
            message(STATUS "Using bundled SDL2 at ${_dir}")
            link_directories(${SDL2_LIB_DIR})
            break()
        endif()
    endforeach()
endif()

if(NOT SDL2_FOUND)
    message(FATAL_ERROR
        "SDL2 not found. Install SDL2 (pacman/apt/brew) or drop a copy "
        "into ./external/SDL2-<version>/. See SDL2_SETUP.md.")
endif()

find_package(OpenMP)

# ---------------------------------------------------------------------------
# On MinGW, statically link libgcc/libstdc++/winpthread so the resulting .exe
# is self-contained (no libgcc_s_seh-1.dll / libstdc++-6.dll dependency).
# ---------------------------------------------------------------------------
if(MINGW)
    add_link_options(
        -static-libgcc
        -static-libstdc++
        -Wl,-Bstatic,--whole-archive,-lwinpthread,-lgomp,--no-whole-archive,-Bdynamic
    )
endif()

# ---------------------------------------------------------------------------
# Primary target — interactive SDL app (Blueprint Edition)
# ---------------------------------------------------------------------------
add_executable(renderer_blueprint
    src/app/main_app_blueprint.cpp
    src/image/tgaimage.cpp
)
target_include_directories(renderer_blueprint PRIVATE
    src
    ${SDL2_INCLUDE_DIRS}
)
target_link_libraries(renderer_blueprint PRIVATE ${SDL2_LIBRARIES})
if(OpenMP_CXX_FOUND)
    target_link_libraries(renderer_blueprint PRIVATE OpenMP::OpenMP_CXX)
endif()

# ---------------------------------------------------------------------------
# Secondary target — offline TGA demo (no SDL dependency)
# ---------------------------------------------------------------------------
add_executable(renderer_demo
    src/main.cpp
    src/image/tgaimage.cpp
)
target_include_directories(renderer_demo PRIVATE src)
if(OpenMP_CXX_FOUND)
    target_link_libraries(renderer_demo PRIVATE OpenMP::OpenMP_CXX)
endif()

# On Windows, copy SDL2.dll next to the exe so it actually runs.
if(WIN32 AND EXISTS "${CMAKE_SOURCE_DIR}/SDL2.dll")
    add_custom_command(TARGET renderer_blueprint POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/SDL2.dll"
                $<TARGET_FILE_DIR:renderer_blueprint>)
endif()

# Copy assets (ghost.obj etc) next to the exe
if(EXISTS "${CMAKE_SOURCE_DIR}/assets")
    add_custom_command(TARGET renderer_blueprint POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/assets"
                "$<TARGET_FILE_DIR:renderer_blueprint>/assets")
endif()

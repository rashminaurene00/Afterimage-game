cmake_minimum_required(VERSION 3.16)
project(Afterimage VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- raylib -------------------------------------------------------------------------
# 1) Use an installed raylib if CMake can find it (vcpkg, MSYS2 pacman, manual install).
#    (skipped when -DAFTERIMAGE_FETCH_RAYLIB=ON or -DRAYLIB_SOURCE_DIR=... is given)
# 2) Otherwise download raylib 5.5 source and build it with the game (needs internet once).
option(AFTERIMAGE_FETCH_RAYLIB "Always build raylib from source (fully static .exe)" OFF)
set(AFTERIMAGE_RAYLIB_FROM_SOURCE OFF)
if(NOT AFTERIMAGE_FETCH_RAYLIB AND NOT DEFINED RAYLIB_SOURCE_DIR)
    find_package(raylib 5.0 QUIET)
endif()
if(NOT raylib_FOUND)
    set(AFTERIMAGE_RAYLIB_FROM_SOURCE ON)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    if(DEFINED RAYLIB_SOURCE_DIR)
        # Offline option: cmake -DRAYLIB_SOURCE_DIR=C:/path/to/raylib-5.5 ..
        add_subdirectory(${RAYLIB_SOURCE_DIR} ${CMAKE_BINARY_DIR}/raylib-build)
    else()
        FetchContent_Declare(raylib
            URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(raylib)
    endif()
endif()

# --- game ---------------------------------------------------------------------------
add_executable(afterimage
    src/main.cpp
    src/Game.cpp
    src/Render.cpp
    src/UI.cpp
    src/World.cpp
    src/Levels.cpp
    src/Player.cpp
    src/Enemy.cpp
    src/Pathfinding.cpp
    src/Afterimage.cpp
    src/AtomicFile.cpp
    src/Art.cpp
)
target_include_directories(afterimage PRIVATE src)
target_link_libraries(afterimage PRIVATE raylib)

if(WIN32)
    target_link_libraries(afterimage PRIVATE opengl32 gdi32 winmm)
endif()
if(MSVC)
    target_compile_options(afterimage PRIVATE /W3 /utf-8)
else()
    target_compile_options(afterimage PRIVATE -Wall -Wextra)
endif()
if(MINGW AND AFTERIMAGE_RAYLIB_FROM_SOURCE)
    # Static runtime so the .exe runs on PCs without MinGW installed.
    target_link_options(afterimage PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Copy assets next to the executable on EVERY build (not only when the .exe relinks),
# so newly rendered Blender sprites are always picked up.
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:afterimage>/assets
    COMMENT "Copying assets/ next to the executable")
add_dependencies(copy_assets afterimage)

# A shared MSYS2 raylib build depends on its compiler/runtime DLLs. Include the
# exact sibling runtime files in `cmake --install` packages. A raylib build
# compiled from source uses the static runtime flags above and needs no MinGW
# runtime DLLs.
install(TARGETS afterimage RUNTIME DESTINATION .)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)
if(MINGW AND NOT AFTERIMAGE_RAYLIB_FROM_SOURCE)
    get_filename_component(AFTERIMAGE_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(AFTERIMAGE_RUNTIME_DLL
            libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll libraylib.dll glfw3.dll)
        if(EXISTS "${AFTERIMAGE_MINGW_BIN}/${AFTERIMAGE_RUNTIME_DLL}")
            install(FILES "${AFTERIMAGE_MINGW_BIN}/${AFTERIMAGE_RUNTIME_DLL}" DESTINATION .)
        else()
            message(WARNING "Runtime DLL not found for package: ${AFTERIMAGE_MINGW_BIN}/${AFTERIMAGE_RUNTIME_DLL}")
        endif()
    endforeach()
endif()

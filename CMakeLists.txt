cmake_minimum_required(VERSION 3.16)
project(C+craft LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ---------------------------------------------------------------------------
# Dependency layout (all relative to the project source dir):
#   deps/glad/include  +  deps/glad/src/gl.c       (GL loader, generated 4.3 core)
#   deps/glm           (header-only OpenGL Mathematics)
#   deps/glfw-install  (pre-built static GLFW + headers)
#   deps/x11           (X11 dev headers not present in the build sandbox)
#
# Set CRAFT_USE_LOCAL_DEPS=OFF to use system-installed GLFW/GLM/OpenGL via
# find_package() instead.
# ---------------------------------------------------------------------------
option(CRAFT_USE_LOCAL_DEPS "Use pre-built local dependencies under deps/" ON)

set(PROJECT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/glm
)

set(PROJECT_SOURCES
    src/Main.cpp
    src/Engine.cpp
    src/Chunk.cpp
    src/Camera.cpp
    src/Shader.cpp
    src/Block.cpp
    src/Noise.cpp
    src/TextureAtlas.cpp
    src/World.cpp
    src/UI.cpp
    src/WorldSave.cpp
)

set(PROJECT_HEADERS
    include/Block.h
    include/Camera.h
    include/Chunk.h
    include/Engine.h
    include/GameState.h
    include/Noise.h
    include/Shader.h
    include/TextureAtlas.h
    include/UI.h
    include/World.h
    include/WorldSave.h
)

if(CRAFT_USE_LOCAL_DEPS)
    set(DEPS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/deps)
    list(APPEND PROJECT_INCLUDE_DIRS
        ${DEPS_DIR}/glfw-install/include
        ${DEPS_DIR}/x11
    )
    # GLAD is compiled straight into the engine.
    list(APPEND PROJECT_SOURCES ${DEPS_DIR}/glad/src/gl.c)

    # Pre-built static GLFW.
    set(GLFW_LIBRARY ${DEPS_DIR}/glfw-install/lib/libglfw3.a)

    # System libraries GLFW statically links against on Linux/X11. X11/Xext
    # have unversioned dev symlinks; the rest are only present as versioned
    # sonames (libXXX.so.1) in this environment, so we link their full paths.
    set(GLFW_SYSTEM_LIBS
        X11 Xext
        rt m pthread dl
    )
    foreach(_lib GL Xrandr Xinerama Xi Xcursor GLU)
        if(EXISTS "/usr/lib/x86_64-linux-gnu/lib${_lib}.so.1")
            list(APPEND GLFW_SYSTEM_LIBS
                 "/usr/lib/x86_64-linux-gnu/lib${_lib}.so.1")
        endif()
    endforeach()
else()
    find_package(GLFW3 REQUIRED)
    find_package(OpenGL REQUIRED)
    find_package(glm REQUIRED)
    list(APPEND PROJECT_INCLUDE_DIRS ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS})
    set(GLFW_LIBRARY glfw)
    set(GLFW_SYSTEM_LIBS )
endif()

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDE_DIRS})

# Compile flags: warn heavily, optimise.
target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -O2 -fomit-frame-pointer>
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    # GLAD provides all OpenGL declarations; tell GLFW not to include GL/gl.h
    # (which is unavailable in this sandbox).
    GLFW_INCLUDE_NONE
)

# Force X11/GL shared libraries into DT_NEEDED: GLFW and GLAD are linked
# statically, so the linker would otherwise drop libX11/libXcursor/libGL
# (--as-needed is the default on Debian/Ubuntu) even though GLFW calls them
# at runtime, which would crash inside glfwInit().
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(${PROJECT_NAME} PRIVATE -Wl,--no-as-needed)
endif()

# Link GLFW + OpenGL + system X11/libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${GLFW_LIBRARY}
    ${GLFW_SYSTEM_LIBS}
)

# Copy shaders next to the executable at build time so relative paths work.
configure_file(shaders/default.vert ${CMAKE_CURRENT_BINARY_DIR}/shaders/default.vert COPYONLY)
configure_file(shaders/default.frag ${CMAKE_CURRENT_BINARY_DIR}/shaders/default.frag COPYONLY)

# Place the executable in the project root for convenience.
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

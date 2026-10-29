cmake_minimum_required(VERSION 3.20)
project(wave_ca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

if(MINGW)
    find_path(GLEW_INCLUDE_DIR GL/glew.h REQUIRED)
    find_library(GLEW_LIBRARY NAMES glew32 REQUIRED)
    add_library(wave_ca_glew STATIC IMPORTED)
    set_target_properties(wave_ca_glew PROPERTIES
        IMPORTED_LOCATION "${GLEW_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GLEW_INCLUDE_DIR}"
        INTERFACE_COMPILE_DEFINITIONS GLEW_STATIC
    )
else()
    find_package(GLEW REQUIRED)
    add_library(wave_ca_glew ALIAS GLEW::GLEW)
endif()

add_executable(wave_ca
    src/main.cpp
)

target_link_libraries(wave_ca PRIVATE
    glfw
    wave_ca_glew
    glm::glm
    OpenGL::GL
)

if(MINGW)
    # Keep the release archive self-contained; Windows supplies the remaining
    # system DLLs (OpenGL, GDI, User, Shell, and the C runtime).
    target_link_options(wave_ca PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Keep shader paths stable when launching from the build directory.
target_compile_definitions(wave_ca PRIVATE
    WAVE_CA_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

add_custom_command(TARGET wave_ca POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
            "$<TARGET_FILE_DIR:wave_ca>/shaders"
    COMMENT "Copying shaders beside the executable"
)

install(TARGETS wave_ca RUNTIME DESTINATION .)
install(DIRECTORY shaders DESTINATION .)
install(FILES LICENSE DESTINATION .)

cmake_minimum_required(VERSION 3.8.0)

# ccls compile_commands.json
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(ProjectId ${CMAKE_CURRENT_SOURCE_DIR} NAME)
project(${ProjectId} C)

set(CMAKE_C_STANDARD 11)

find_package(SDL2 REQUIRED CONFIG REQUIRED COMPONENTS SDL2)
find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)
find_package(Vulkan REQUIRED COMPONENTS glslangValidator)

file(GLOB_RECURSE PROJECT_SOURCE_FILES "src/*")
add_executable(${PROJECT_NAME} ${PROJECT_SOURCE_FILES})
target_compile_options(
    ${PROJECT_NAME} PRIVATE
    -Wall -Wextra -O3 $<$<CONFIG:Debug>:-pg>
)
if (CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE -fcolor-diagnostics)
elseif (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -fdiagnostics-color=always)
endif()

if (TARGET SDL2::SDL2main)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2main)
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 Vulkan::Vulkan m)
target_include_directories(${PROJECT_NAME} PRIVATE "include/" "extern/cglm/include/")

# https://gist.github.com/evilactually/a0d191701cb48f157b05be7f74d79396
# GLOB instead of GLOB_RECURSE because subdirs are pain
file(GLOB SHADER_SOURCE_FILES LIST_DIRECTORIES false "data/shaders/*")
set(SHADERS_COMPILED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data/shaders_compiled")
foreach (SHADER_SOURCE ${SHADER_SOURCE_FILES})
    get_filename_component(SHADER "${SHADER_SOURCE}" NAME)
    set(SHADER_OUT "${SHADERS_COMPILED_DIR}/${SHADER}.spv")
    add_custom_command(
        OUTPUT ${SHADER_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_COMPILED_DIR}"
        COMMAND ${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE} -V "${SHADER_SOURCE}" -o "${SHADER_OUT}"
        DEPENDS ${SHADER_SOURCE}
    )
    list(APPEND SHADER_BINARY_FILES ${SHADER_OUT})
endforeach(SHADER_SOURCE)
add_custom_target(Shaders DEPENDS ${SHADER_BINARY_FILES})
add_dependencies(${PROJECT_NAME} Shaders)


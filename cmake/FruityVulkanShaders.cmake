# One compiler and target environment for local and CI Vulkan shader builds.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(FRUITY_GLSLC NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin"
        "$ENV{VCPKG_INSTALLATION_ROOT}/installed/x64-windows/tools/shaderc"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/shaderc"
    REQUIRED)
set(FRUITY_VULKAN_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/vulkan-shaders")
set(_fruity_shader_sources "${CMAKE_CURRENT_SOURCE_DIR}/src/MphRead.Native/Shaders.cpp")
set(_fruity_shader_generator "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-vulkan-scene-shaders.py")
set(_fruity_generated_glsl)
set(_fruity_generated_spirv)
foreach(_program main composite cel shift)
    foreach(_stage vert frag)
        list(APPEND _fruity_generated_glsl "${FRUITY_VULKAN_SHADER_DIR}/${_program}.${_stage}")
        list(APPEND _fruity_generated_spirv "${FRUITY_VULKAN_SHADER_DIR}/${_program}.${_stage}.spv")
    endforeach()
endforeach()
add_custom_command(
    OUTPUT ${_fruity_generated_glsl} "${FRUITY_VULKAN_SHADER_DIR}/bindings.json"
    COMMAND Python3::Interpreter "${_fruity_shader_generator}"
        --source "${_fruity_shader_sources}" --output "${FRUITY_VULKAN_SHADER_DIR}"
    DEPENDS "${_fruity_shader_generator}" "${_fruity_shader_sources}"
    VERBATIM)
foreach(_source IN LISTS _fruity_generated_glsl)
    add_custom_command(
        OUTPUT "${_source}.spv"
        COMMAND "${FRUITY_GLSLC}" --target-env=vulkan1.3 --target-spv=spv1.5 -O0
            "${_source}" -o "${_source}.spv"
        DEPENDS "${_source}" "${FRUITY_GLSLC}"
        VERBATIM)
endforeach()
set(_fruity_shader_header "${FRUITY_VULKAN_SHADER_DIR}/FruityVulkanSceneShaders.hpp")
add_custom_command(
    OUTPUT "${_fruity_shader_header}"
    COMMAND Python3::Interpreter "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-vulkan-scene-shaders.py"
        --directory "${FRUITY_VULKAN_SHADER_DIR}" --output "${_fruity_shader_header}"
    DEPENDS ${_fruity_generated_spirv} "${FRUITY_VULKAN_SHADER_DIR}/bindings.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed-vulkan-scene-shaders.py"
    VERBATIM)
add_custom_target(fruity_vulkan_shaders DEPENDS "${_fruity_shader_header}")
target_sources(fruity_mphread_native PRIVATE "${_fruity_shader_header}")
target_include_directories(fruity_mphread_native PRIVATE "${FRUITY_VULKAN_SHADER_DIR}")
add_dependencies(fruity_mphread_native fruity_vulkan_shaders)

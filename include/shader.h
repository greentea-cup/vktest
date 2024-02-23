#ifndef SHADER_H
#define SHADER_H
#include "vulkan/vulkan.h"
#include <stdint.h>

typedef struct AShader {
    VkShaderModule module;
    VkShaderStageFlagBits stage;
} AShader;

AShader AShader_from_path(VkDevice device, char const *path, VkShaderStageFlagBits stage);

AShader AShader_from_code(
    VkDevice device, uint32_t codeSize, uint32_t const *code, VkShaderStageFlagBits stage);

void AShader_destroy(VkDevice device, AShader shaderToDestroy);

#endif

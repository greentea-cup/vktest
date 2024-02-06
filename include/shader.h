#ifndef SHADER_H
#define SHADER_H
#include <stdint.h>
#include "vulkan/vulkan.h"

typedef struct {
    uint32_t size;
    uint32_t *code;
} ShaderCode;

ShaderCode read_shader(char const *path);

VkShaderModule create_shader_module(VkDevice device, ShaderCode shader);

void destroy_shader_module(VkDevice device, VkShaderModule sm);
#endif

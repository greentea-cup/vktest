#include "shader.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

ShaderCode read_shader(char const *path) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        eprintf(MSG_ERROR("read_fp: cannot open '%s'"), path);
        return (ShaderCode){.size = 0, .code = NULL};
    }
    fseek(fp, 0, SEEK_END);
    uint32_t fpSize = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    ARR_ALLOC(char, data, fpSize);
    fread(data, 1, fpSize, fp);
    fclose(fp);
    return (ShaderCode){.size = fpSize, .code = (uint32_t *)data};
}

VkShaderModule create_shader_module(VkDevice device, ShaderCode shader) {
    VkShaderModuleCreateInfo smCInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .codeSize = shader.size,
        .pCode = shader.code};
    VkShaderModule sm;
    vkCreateShaderModule(device, &smCInfo, VK_NULL_HANDLE, &sm);
    return sm;
}

void destroy_shader_module(VkDevice device, VkShaderModule sm) { vkDestroyShaderModule(device, sm, VK_NULL_HANDLE); }

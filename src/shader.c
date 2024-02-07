#include "shader.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>

/*
 * AShader on success
 * AShader{0} on fail
 * `code` is not owned (not freed after process)
 */
AShader AShader_from_code(
    VkDevice device, uint32_t codeSize, uint32_t const *code, VkShaderStageFlagBits stage) {
    VkShaderModuleCreateInfo moduleInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize = codeSize, .pCode = code};
    VkShaderModule module;
    if (vkCreateShaderModule(device, &moduleInfo, VK_NULL_HANDLE, &module) != VK_SUCCESS) {
        return (AShader){.module = NULL};
    }
    return (AShader){.module = module, .stage = stage};
}

/*
 * AShader on success
 * AShader{0} on fail
 * `path` is not owned (not freed after process)
 */
AShader AShader_from_path(VkDevice device, char const *path, VkShaderStageFlagBits stage) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        eprintf(MSG_ERROR("read_fp: cannot open '%s'"), path);
        return (AShader){.module = NULL};
    }
    fseek(fp, 0, SEEK_END);
    uint32_t fpSize = (uint32_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    ARR_ALLOC(uint32_t, data, fpSize);
    fread(data, 1, fpSize, fp);
    fclose(fp);
    AShader result = AShader_from_code(device, fpSize, data, stage);
    free(data);
    return result;
}

void AShader_destroy(VkDevice device, AShader shader) {
    // NULL is ok
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/vkDestroyShaderModule.html
    vkDestroyShaderModule(device, shader.module, NULL);
}

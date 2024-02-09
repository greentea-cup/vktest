#include "vertex.h"
#include "utils.h"
#include "vulkan/vulkan.h"

VkVertexInputBindingDescription Vertex_binding(uint32_t binding) {
    return (VkVertexInputBindingDescription){
        .binding = binding, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
}

VkVertexInputAttributeDescription *Vertex_attributes(uint32_t binding, uint32_t *out_count) {
    *out_count = 3;
    ARR_ALLOC(VkVertexInputAttributeDescription, result, 3);
    result[0] = (VkVertexInputAttributeDescription){
        .binding = binding,
        .location = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, pos)};
    result[1] = (VkVertexInputAttributeDescription){
        .binding = binding,
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color)};
    result[2] = (VkVertexInputAttributeDescription){
        .binding = binding,
        .location = 2,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, texCoord)};
    return result;
}

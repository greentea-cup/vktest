#ifndef VERTEX_H
#define VERTEX_H

#include <vulkan/vulkan.h>

typedef struct Vertex {
    float pos[2];
    float color[3];
} Vertex;

VkVertexInputBindingDescription Vertex_binding(uint32_t binding);

VkVertexInputAttributeDescription *Vertex_attributes(uint32_t binding, uint32_t *out_count);

#endif

#ifndef VERTEX_H
#define VERTEX_H

#include <cglm/vec2.h>
#include <cglm/vec3.h>
#include <vulkan/vulkan.h>

typedef struct Vertex {
    vec2 pos;
    vec3 color;
    vec2 texCoord;
} Vertex;

VkVertexInputBindingDescription Vertex_binding(uint32_t binding);

VkVertexInputAttributeDescription *Vertex_attributes(uint32_t binding, uint32_t *out_count);

#endif

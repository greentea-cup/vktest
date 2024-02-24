#ifndef BUFFER_H
#define BUFFER_H

#include "vulkan/vulkan.h"

VkDescriptorSetLayout A_create_descriptor_set_layout(VkDevice device);

typedef struct FillBufferParams {
    uint32_t bufferOffset;
    uint32_t dataOffset;
    uint32_t size;
} FillBufferParams;

/*
 * returns valid memory type index on success
 * -1 if not found
 */
int32_t find_memory_type(
    VkPhysicalDevice pdevice, uint32_t typeFilter, VkMemoryPropertyFlagBits properties);

/*
 * Returns valid VkBuffer,
 *  VkDeviceMemory in out_bufferMemory,
 *  and used memory type index in out_memoryTypeIndex
 *  on success
 * If out_bufferMemory is NULL, no memory is allocated nor bound
 * If out_memoryTypeIndex is NULL, memory type index is not returned
 * NULL on failure
 */
VkBuffer create_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize, VkBufferUsageFlags bufferUsage,
    VkMemoryPropertyFlagBits memoryProperties, VkDeviceMemory *out_bufferMemory);

/*
 * Returns valid VkBuffer and VkDeviceMemory in out_bufferMemory on success
 * NULL on failure
 */
VkBuffer create_staging_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory);

/*
 * Returns valid VkBuffer and VkDeviceMemory in out_bufferMemory on success
 * NULL on failure
 */
VkBuffer create_vertex_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory);

/*
 * Returns valid VkBuffer and VkDeviceMemory in out_bufferMemory on success
 * NULL on failure
 */
VkBuffer create_index_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory);

/*
 * Returns valid VkBuffer[count],
 *  VkDeviceMemory[count] in out_bufferMemories,
 *  and mapped memory areas in out_buffersMapped[count]
 *  on success
 * NULL on failure
 * Allocated one chunk of memory for all buffers
 */
VkBuffer *create_uniform_buffers(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t count, uint32_t buffersSize,
    VkDeviceMemory **out_bufferMemories, void ***out_buffersMapped);

/*
 * 0 on success
 * 1 if map memory failed
 */
int fill_buffer(VkDevice device, VkDeviceMemory bufferMemory, void *data, FillBufferParams args);

#endif

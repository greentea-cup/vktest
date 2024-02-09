#ifndef IMAGE_H
#define IMAGE_H

#include "vulkan/vulkan.h"

VkImage create_image(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    VkDeviceMemory *out_imageMemory);

void transition_image_layout(
    VkCommandBuffer cb, VkImage image, VkFormat format, VkImageLayout oldLayout,
    VkImageLayout newLayout);

void copy_buffer_to_image(
    VkCommandBuffer cb, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

VkImage create_texture_image(
    VkDevice device, VkPhysicalDevice pdevice, char const *image_path, VkCommandPool commandPool,
    VkQueue drawQueue, VkDeviceMemory *out_imageMemory);

#endif

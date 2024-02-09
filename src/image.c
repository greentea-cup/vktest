#include "image.h"
#include "buffer.h"
#include "command.h"
#include "lodepng.h"
#include "utils.h"

VkImage create_image(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t width, uint32_t height, VkFormat format,
    VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
    VkDeviceMemory *out_imageMemory) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = width,
        .extent.height = height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = format,
        .tiling = tiling,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT};
    VkImage image;
    VkResult res = vkCreateImage(device, &imageInfo, NULL, &image);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create image: %d"), res);
        goto no_image;
    }
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    int32_t memoryTypeIndex = find_memory_type(pdevice, memReqs.memoryTypeBits, properties);
    if (memoryTypeIndex == -1) {
        eprintf(MSG_ERROR("cannot find suitable memory type for image"));
        goto no_suitable_memory;
    }
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryTypeIndex};
    VkDeviceMemory imageMemory;
    res = vkAllocateMemory(device, &allocInfo, NULL, &imageMemory);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot allocate memory for image: %d"), res);
        goto no_image_memory;
    }
    vkBindImageMemory(device, image, imageMemory, 0);
    *out_imageMemory = imageMemory;
    return image;
no_image_memory:
no_suitable_memory:
    vkDestroyImage(device, image, NULL);
no_image:
    return NULL;
}

void transition_image_layout(
    VkCommandBuffer cb, VkImage image, VkFormat format, VkImageLayout oldLayout,
    VkImageLayout newLayout) {

    VkPipelineStageFlags srcStage, dstStage;
    VkAccessFlags srcAccessMask, dstAccessMask;

    switch (oldLayout) {
    default: goto invalid_old_layout;
    case VK_IMAGE_LAYOUT_UNDEFINED:
        switch (newLayout) {
        default: goto invalid_new_layout;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            srcAccessMask = 0;
            dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        switch (newLayout) {
        default: goto invalid_new_layout;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        break;
    }

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // if no ownership transfer
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // ^^^
        .image = image,
        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .subresourceRange.baseMipLevel = 0,
        .subresourceRange.levelCount = 1,
        .subresourceRange.baseArrayLayer = 0,
        .subresourceRange.layerCount = 1};

    vkCmdPipelineBarrier(
        cb, srcStage, dstStage, 0, // flags
        0, NULL,                   // memory barrier
        0, NULL,                   // buffer memory barrier
        1, &barrier                // image memory barrier
    );
    return;

invalid_old_layout:
    eprintf(MSG_ERROR("invalid oldLayout for transition_image_layout: %d"), oldLayout);
    return;
invalid_new_layout:
    eprintf(MSG_ERROR("invalid newLayout for transition_image_layout: %d"), newLayout);
    return;
}

void copy_buffer_to_image(
    VkCommandBuffer cb, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .imageSubresource.mipLevel = 0,
        .imageSubresource.baseArrayLayer = 0,
        .imageSubresource.layerCount = 1,
        .imageOffset = {0, 0, 0},
        .imageExtent.width = width,
        .imageExtent.height = height,
        .imageExtent.depth = 1
    };
    vkCmdCopyBufferToImage(cb, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

VkImage create_texture_image(
    VkDevice device, VkPhysicalDevice pdevice, char const *image_path, VkCommandPool commandPool,
    VkQueue drawQueue, VkDeviceMemory *out_imageMemory) {
    uint8_t *image;
    uint32_t width, height;
    uint32_t error = lodepng_decode32_file(&image, &width, &height, image_path);
    if (error) {
        eprintf(MSG_ERROR("cannot load image '%s': %d"), image_path, error);
        goto no_image;
    }
    VkDeviceSize imageSize = width * height * 4;
    VkDeviceMemory sImageMemory;
    VkBuffer sImageBuffer = create_staging_buffer(device, pdevice, imageSize, &sImageMemory);
    if (sImageBuffer == NULL) {
        eprintf(MSG_ERROR("failed to create staging buffer for image"));
        goto no_staging_buffer;
    }
    {
        void *data;
        vkMapMemory(device, sImageMemory, 0, imageSize, 0, &data);
        memcpy(data, image, imageSize);
        vkUnmapMemory(device, sImageMemory);
    }
    VkDeviceMemory textureImageMemory;
    VkImage textureImage = create_image(
        device, pdevice, width, height, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &textureImageMemory);
    if (textureImage == NULL) {
        eprintf(MSG_ERROR("failed to create image"));
        goto no_texture_image;
    }

    {
        VkCommandBuffer cb = cmd_begin_one_time(device, commandPool);
        transition_image_layout(
            cb, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copy_buffer_to_image(cb, sImageBuffer, textureImage, width, height);
        transition_image_layout(
            cb, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        cmd_end_one_time(device, commandPool, drawQueue, cb);
    }

    vkFreeMemory(device, sImageMemory, NULL);
    vkDestroyBuffer(device, sImageBuffer, NULL);
    free(image);

    *out_imageMemory = textureImageMemory;
    return textureImage;

no_texture_image:
    vkFreeMemory(device, sImageMemory, NULL);
    vkDestroyBuffer(device, sImageBuffer, NULL);
no_staging_buffer:
    free(image);
no_image:
    return NULL;
}

static int create_tetxure_image_view() {
    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = compMap,
        .subresourceRange = imageSubresRange};
    vulkan->swcImageViews = ARR_INPLACE_ALLOC(VkImageView, vulkan->swcImageCount);

    VkResult res;

    res = vkCreateImageView(device, &viewInfo, NULL, &imageView);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create image view: %d"), res);
        return 1;
    }
}
return 0;
}

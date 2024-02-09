#include "buffer.h"
#include "utils.h"
#include <string.h>

VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device) {
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = NULL};
    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &binding};
    VkDescriptorSetLayout descriptorSetLayout;
    VkResult res = vkCreateDescriptorSetLayout(device, &info, NULL, &descriptorSetLayout);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create descriptor set layout: %d"), res);
        return NULL;
    }
    return descriptorSetLayout;
}

int32_t find_memory_type(
    VkPhysicalDevice pdevice, uint32_t typeFilter, VkMemoryPropertyFlagBits properties) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(pdevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            uint32_t propFlags = memProps.memoryTypes[i].propertyFlags & properties;
            if (propFlags == properties) return (int32_t)i;
        }
    }
    return -1;
}

VkBuffer create_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize, VkBufferUsageFlags bufferUsage,
    VkMemoryPropertyFlagBits memoryProperties, VkDeviceMemory *out_bufferMemory) {
    VkBufferCreateInfo bcInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = bufferSize,
        .usage = bufferUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer buffer;
    VkResult res = vkCreateBuffer(device, &bcInfo, NULL, &buffer);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create buffer: %d"), res);
        return NULL;
    }
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, buffer, &memReqs);
    int32_t memoryTypeIndex = find_memory_type(pdevice, memReqs.memoryTypeBits, memoryProperties);
    if (memoryTypeIndex == -1) {
        eprintf(MSG_ERROR("no suitable memory type"));
        goto cleanup;
    }
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryTypeIndex};
    if (out_bufferMemory != NULL) {
        VkDeviceMemory bufMem = NULL;
        res = vkAllocateMemory(device, &allocInfo, NULL, &bufMem);
        if (res != VK_SUCCESS) {
            eprintf(MSG_ERROR("cannot allocate memory: %d"), res);
            goto cleanup;
        }
        res = vkBindBufferMemory(device, buffer, bufMem, 0);
        if (res != VK_SUCCESS) {
            eprintf(MSG_ERROR("cannot bind buffer memory: %d"), res);
            vkFreeMemory(device, bufMem, NULL);
            goto cleanup;
        }
        *out_bufferMemory = bufMem;
    }
    return buffer;
cleanup:
    vkDestroyBuffer(device, buffer, NULL);
    return NULL;
}

VkBuffer create_staging_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlagBits memProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer(device, pdevice, bufferSize, usage, memProps, out_bufferMemory);
}

VkBuffer create_vertex_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer(device, pdevice, bufferSize, usage, memProps, out_bufferMemory);
}

VkBuffer create_index_buffer(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t bufferSize,
    VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer(device, pdevice, bufferSize, usage, memProps, out_bufferMemory);
}

VkBuffer *create_uniform_buffers(
    VkDevice device, VkPhysicalDevice pdevice, uint32_t count, uint32_t buffersSize,
    VkDeviceMemory **out_bufferMemories, void ***out_buffersMapped) {
    ARR_ALLOC(VkBuffer, buffers, count);
    ARR_ALLOC(VkDeviceMemory, memories, count);
    ARR_ALLOC(void *, mappedMemories, count);
    uint32_t successful = 0;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlagBits memProps =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    // create buffers
    for (uint32_t i = 0; i < count; i++) {
        VkBuffer buffer =
            create_buffer(device, pdevice, buffersSize, usage, memProps, memories + i);
        if (buffer == NULL) {
            successful = i; // this buffer is not created
            goto cleanup;
        }
        buffers[i] = buffer;
    }
    // map memory to areas
    uint32_t mapSuccessful = 0;
    for (uint32_t i = 0; i < count; i++) {
        VkResult res = vkMapMemory(device, memories[i], 0, buffersSize, 0, mappedMemories + i);
        if (res != VK_SUCCESS) {
            eprintf(MSG_ERROR("cannot map buffer memory: %d"), res);
            successful = count;
            mapSuccessful = i;
            goto unmap;
        }
    }
    *out_bufferMemories = memories;
    *out_buffersMapped = mappedMemories;
    return buffers;
unmap:
    for (uint32_t i = 0; i < mapSuccessful; i++) { vkUnmapMemory(device, memories[i]); }
cleanup:
    for (uint32_t i = 0; i < successful; i++) { vkDestroyBuffer(device, buffers[i], NULL); }
    free(mappedMemories);
    free(memories);
    free(buffers);
    return NULL;
}

int fill_buffer(VkDevice device, VkDeviceMemory bufferMemory, void *data, FillBufferParams args) {
    void *mapData;
    VkResult res = vkMapMemory(device, bufferMemory, args.bufferOffset, args.size, 0, &mapData);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("map memory failed: %d"), res);
        return 1;
    }
    memcpy(mapData, data, (size_t)args.size);
    vkUnmapMemory(device, bufferMemory);
    return 0;
}

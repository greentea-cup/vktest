#include "SDL.h"
#include "my_vulkan.h"
#include "pipeline.h"
#include "shader.h"
#include "utils.h"
#include "vertex.h"
#include "vulkan/vulkan.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <time.h>

// #define BIG_INDEX_T

#ifdef BIG_INDEX_T
typedef uint32_t VertexIdx;
#define VulkanIndexType VK_INDEX_TYPE_UINT32
#else
typedef uint16_t VertexIdx;
#define VulkanIndexType VK_INDEX_TYPE_UINT16
#endif

VkDescriptorSetLayout create_descriptor_set_layout(Vulkan *vulkan) {
    VkDescriptorSetLayoutBinding binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .pImmutableSamplers = VK_NULL_HANDLE};
    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .bindingCount = 1,
        .pBindings = &binding};
    VkDescriptorSetLayout descriptorSetLayout;
    VkResult res = vkCreateDescriptorSetLayout(vulkan->device, &info, VK_NULL_HANDLE, &descriptorSetLayout);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create descriptor set layout: %d"), res);
        return VK_NULL_HANDLE;
    }
    return descriptorSetLayout;
}

typedef struct {
    VkBuffer src;
    VkBuffer dst;
    VkDeviceSize srcOffset;
    VkDeviceSize dstOffset;
    VkDeviceSize size;
} VulkanCopyBufferParams;
/*
 * 0 on success
 * 1 if buffer copy failed
 */
int copy_buffer(Vulkan *vulkan, VulkanCopyBufferParams args) {
    if (args.size == 0) return 0;
    VkCommandBufferAllocateInfo cbAInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = vulkan->commandPool,
        .commandBufferCount = 1};

    VkCommandBuffer cb;
    vkAllocateCommandBuffers(vulkan->device, &cbAInfo, &cb);

    VkCommandBufferBeginInfo cbBInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = VK_NULL_HANDLE};
    vkBeginCommandBuffer(cb, &cbBInfo);

    VkBufferCopy copyRegion = {.srcOffset = args.srcOffset, .dstOffset = args.dstOffset, .size = args.size};
    vkCmdCopyBuffer(cb, args.src, args.dst, 1, &copyRegion);
    vkEndCommandBuffer(cb);

    VkSubmitInfo sInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = VK_NULL_HANDLE,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = VK_NULL_HANDLE,
        .pWaitDstStageMask = VK_NULL_HANDLE,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = VK_NULL_HANDLE};
    vkQueueSubmit(vulkan->drawQueue, 1, &sInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(vulkan->drawQueue);

    vkFreeCommandBuffers(vulkan->device, vulkan->commandPool, 1, &cb);
    return 0;
}

/*
 * 0 on success
 * 1 if proper memory type not found
 * returns memory type to out_memoryType
 */
int find_memory_type(
    Vulkan *vulkan, uint32_t typeFilter, VkMemoryPropertyFlagBits properties, uint32_t *out_memoryType) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(vulkan->pdevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && ((memProps.memoryTypes[i].propertyFlags & properties) == properties)) {
            *out_memoryType = i;
            return 0;
        }
    }
    return 1;
}

/*
 * 0 on success
 * 1 if no suitable memory type found
 * 2 if cannot allocate memory
 */
int create_buffer(
    Vulkan *vulkan, uint32_t bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlagBits memoryProperties,
    VkBuffer *out_buffer, VkDeviceMemory *out_bufferMemory) {
    int ret = 0;
    VkBufferCreateInfo bcInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .size = bufferSize,
        .usage = bufferUsage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkBuffer buffer;
    VkDeviceMemory bufMem = VK_NULL_HANDLE;
    vkCreateBuffer(vulkan->device, &bcInfo, VK_NULL_HANDLE, &buffer);
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(vulkan->device, buffer, &memReqs);
    // find memory type
    uint32_t memoryType;
    if (find_memory_type(vulkan, memReqs.memoryTypeBits, memoryProperties, &memoryType)) {
        eprintf(MSG_ERROR("no suitable memory type"));
        ret = 1;
        goto cleanup;
    }
    // allocate buffer
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryType};

    VkResult res = vkAllocateMemory(vulkan->device, &allocInfo, VK_NULL_HANDLE, &bufMem);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot allocate memory: %d"), res);
        ret = 2;
        goto cleanup;
    }
    vkBindBufferMemory(vulkan->device, buffer, bufMem, 0);
    *out_buffer = buffer;
    *out_bufferMemory = bufMem;
    return ret;
cleanup:
    vkDestroyBuffer(vulkan->device, buffer, VK_NULL_HANDLE);
    *out_buffer = VK_NULL_HANDLE;
    *out_bufferMemory = VK_NULL_HANDLE;
    return ret;
}

/*
 * 0 on success
 * 1 on fail
 */
int create_staging_buffer(Vulkan *vulkan, uint32_t bufferSize, VkBuffer *out_buffer, VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer(vulkan, bufferSize, usage, memProps, out_buffer, out_bufferMemory);
}

/*
 * 0 on success
 * 1 on fail
 */
int create_vertex_buffer(Vulkan *vulkan, uint32_t bufferSize, VkBuffer *out_buffer, VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer(vulkan, bufferSize, usage, memProps, out_buffer, out_bufferMemory);
}

/*
 * 0 on success
 * 1 on fail
 */
int create_index_buffer(Vulkan *vulkan, uint32_t bufferSize, VkBuffer *out_buffer, VkDeviceMemory *out_bufferMemory) {
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer(vulkan, bufferSize, usage, memProps, out_buffer, out_bufferMemory);
}

/*
 * 0 on success
 * 1 on fail
 */
int create_uniform_buffers(
    Vulkan *vulkan, uint32_t count, uint32_t buffersSize, VkBuffer *out_buffers, VkDeviceMemory *out_buffersMemories,
    void **out_buffersMapped) {
    for (uint32_t i = 0; i < count; i++) {
        VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlagBits memProps = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        int res = create_buffer(vulkan, buffersSize, usage, memProps, out_buffers + i, out_buffersMemories + i);
        if (res) return res;
        VkResult res2 = vkMapMemory(vulkan->device, out_buffersMemories[i], 0, buffersSize, 0, out_buffersMapped + i);
        if (res2 != VK_SUCCESS) return 1;
    }
    return 0;
}

typedef struct {
    uint32_t bufferOffset;
    uint32_t dataOffset;
    uint32_t size;
} FillBufferParams;
/*
 * 0 on success
 * 1 if map memory failed
 */
int fill_buffer(Vulkan *vulkan, VkDeviceMemory bufferMemory, void *data, FillBufferParams args) {
    void *mapData;
    VkResult res = vkMapMemory(vulkan->device, bufferMemory, args.bufferOffset, args.size, 0, &mapData);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("map memory failed: %d"), res);
        return 1;
    }
    memcpy(mapData, data, (size_t)args.size);
    vkUnmapMemory(vulkan->device, bufferMemory);
    return 0;
}

typedef struct {
    VkBuffer vBuffer;
    VkBuffer iBuffer;
    uint32_t indexCount;
    uint32_t indexOffset;
    VkDescriptorSet *descriptorSets;
} RecordCmdBuffersParams;
void record_command_buffer(
    Vulkan *vulkan, VkPipeline pipeline, VkPipelineLayout plLayout, uint32_t currentFrame, uint32_t imageIndex,
    RecordCmdBuffersParams args) {
    VkCommandBufferBeginInfo cbBInfo = {.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    VkClearValue clearValue = {.color = {.uint32 = {154, 52, 205, 0}}};
    VkRenderPassBeginInfo rpBInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = vulkan->renderPass,
        .framebuffer = vulkan->framebuffers[imageIndex],
        .renderArea = (VkRect2D){.offset = {0, 0}, .extent = vulkan->swcExtent},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };
    VkCommandBuffer cmdBuf = vulkan->commandBuffers[currentFrame];
    VkResult res = vkBeginCommandBuffer(cmdBuf, &cbBInfo);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("vkBeginCommandBuffer %d"), res);
        return;
    }
    vkCmdBeginRenderPass(cmdBuf, &rpBInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdSetViewport(cmdBuf, 0, 1, &vulkan->viewport);
    vkCmdSetScissor(cmdBuf, 0, 1, &vulkan->scissor);
    vkCmdBindVertexBuffers(cmdBuf, 0, 1, &args.vBuffer, (VkDeviceSize[]){0});
    vkCmdBindIndexBuffer(cmdBuf, args.iBuffer, 0, VulkanIndexType);
    vkCmdBindDescriptorSets(
        cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, plLayout, 0, 1, args.descriptorSets + currentFrame, 0, VK_NULL_HANDLE);
    vkCmdDrawIndexed(cmdBuf, args.indexCount, 1, args.indexOffset, 0, 0);
    vkCmdEndRenderPass(cmdBuf);
    res = vkEndCommandBuffer(cmdBuf);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("vkEndCommandBuffer %d"), res);
        return;
    }
}

struct MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
};

int main() {
    // init SDL2
    SDL_Init(SDL_INIT_EVERYTHING);
    // create SDL window
    SDL_Window *window = SDL_CreateWindow(
        "vktest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    Vulkan vulkan = init_vulkan(window);
    if (vulkan.status != VIS_OK) goto vulkan_init_error;
    eprintf(MSG_INFO("Vulkan initialized successfully"));

// shaders
#define SHADER_PATH_PREFIX "./data/shaders_compiled/"
#define SHADER_PATH(x) SHADER_PATH_PREFIX x
    char const *vertShaderPath = SHADER_PATH("main.vert.spv");
    char const *fragShaderPath = SHADER_PATH("main.frag.spv");
    ShaderCode vertShader = read_shader(vertShaderPath);
    ShaderCode fragShader = read_shader(fragShaderPath);
    if (vertShader.size == 0 || fragShader.size == 0) { goto shader_load_error; }
    eprintf(MSG_INFO("Shaders loaded successfully"));
    // shaders == create program
    VkShaderModule vertSM = create_shader_module(vulkan.device, vertShader);
    VkShaderModule fragSM = create_shader_module(vulkan.device, fragShader);
    VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(&vulkan);
    VkPipelineLayout plLayout = VK_NULL_HANDLE; // this thing needs to be 'Destroy'ed too
    VkPipelineLayoutCreateInfo plCInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = VK_NULL_HANDLE};
    VkPipeline graphicsPipeline = create_graphics_pipeline(
        vulkan.device, plCInfo, (VkShaderModule[]){vertSM, fragSM},
        (VkShaderStageFlags[]){VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_FRAGMENT_BIT}, 2, vulkan.renderPass,
        &plLayout);
    // shaders not needed here because they are inside graphicsPipeline
    free(vertShader.code);
    free(fragShader.code);
    destroy_shader_module(vulkan.device, vertSM);
    destroy_shader_module(vulkan.device, fragSM);

#define RGB(x) {(x >> 16 & 0xff) / 256., (x >> 8 & 0xff) / 256., (x & 0xff) / 256.}

    Vertex vertexData[] = {
  // 0
        {.pos = {-1., -1.}, .color = RGB(0x7f7f7f)},
        {.pos = {+1., -1.}, .color = RGB(0x0000ff)},
        {.pos = {-1., +1.}, .color = RGB(0xff0000)},
        {.pos = {+1., +1.}, .color = RGB(0x00ff00)},
 // 1
        {.pos = {-1., -1.}, .color = RGB(0x422245)},
        {.pos = {-1., +1.}, .color = RGB(0xa333f1)},
        {.pos = {+0., +0.}, .color = RGB(0xabcdef)},
        {.pos = {+1., +1.}, .color = RGB(0xb11132)},
        {.pos = {+1., -1.}, .color = RGB(0xa2abf3)},

 // 2
    };
    VertexIdx indices[] = {
        0, 1, 2, 2, 1, 3,                  // 0
        4, 5, 6, 6, 5, 7, 7, 8, 6, 6, 8, 4 // 1
    };
    uint32_t offsets[] = {0, 6};
    uint32_t lengths[] = {6, 12};
    uint32_t index = 0;
    uint32_t presetLength = 2;
    uint32_t bufferSize = sizeof(vertexData); // sizeof(*vertexData) * vertexCount;
    uint32_t indexSize = sizeof(indices);
    // uint32_t vertexCount = indexSize / sizeof(*indices);
    // create buffers
    VkBuffer svBuffer, vBuffer, siBuffer, iBuffer;
    ARR_ALLOC(VkBuffer, uBuffers, vulkan.maxFrames);
    VkDeviceMemory svBufMem, vBufMem, siBufMem, iBufMem;
    ARR_ALLOC(VkDeviceMemory, uBufMems, vulkan.maxFrames);
    ARR_ALLOC(void *, uBufsMapped, vulkan.maxFrames);
    VkDeviceSize uBufSize = sizeof(struct MVP);
    create_staging_buffer(&vulkan, bufferSize, &svBuffer, &svBufMem);
    create_vertex_buffer(&vulkan, bufferSize, &vBuffer, &vBufMem);
    create_staging_buffer(&vulkan, indexSize, &siBuffer, &siBufMem);
    create_index_buffer(&vulkan, indexSize, &iBuffer, &iBufMem);
    create_uniform_buffers(&vulkan, vulkan.maxFrames, uBufSize, uBuffers, uBufMems, uBufsMapped);
    // end create buffers
    // create descriptor pool
    VkDescriptorPoolSize poolSize = {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = vulkan.maxFrames};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .flags = 0,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
        .maxSets = vulkan.maxFrames};
    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(vulkan.device, &poolInfo, VK_NULL_HANDLE, &descriptorPool) != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to create descriptor pool"));
        goto descriptor_pool_failed;
    }
    // end create descriptor pool
    // create descriptor set
    // layouts is duped descriptorSetLayout
    ARR_ALLOC(VkDescriptorSetLayout, layouts, vulkan.maxFrames);
    for (uint32_t i = 0; i < vulkan.maxFrames; i++) { layouts[i] = descriptorSetLayout; }
    VkDescriptorSetAllocateInfo dsAInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = VK_NULL_HANDLE,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = vulkan.maxFrames,
        .pSetLayouts = layouts};
    ARR_ALLOC(VkDescriptorSet, descriptorSets, vulkan.maxFrames);
    if (vkAllocateDescriptorSets(vulkan.device, &dsAInfo, descriptorSets) != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to allocate descriptor sets"));
        goto descriptor_sets_failed;
    }
    uint32_t uBufBinding = 0; // binding index for uniform buffers in shaders
    // populate descriptors
    for (uint32_t i = 0; i < vulkan.maxFrames; i++) {
        VkDescriptorBufferInfo bufInfo = {
            .buffer = uBuffers[i],
            .offset = 0,
            .range = sizeof(struct MVP) // VK_WHOLE_SIZE
        };
        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = VK_NULL_HANDLE,
            .dstSet = descriptorSets[i],
            .dstBinding = uBufBinding,
            .dstArrayElement = 0, // from
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1, // count
            .pBufferInfo = &bufInfo,
            .pImageInfo = VK_NULL_HANDLE,
            .pTexelBufferView = VK_NULL_HANDLE};
        // device, descriptor count to write, which to write, count to copy, which to copy
        vkUpdateDescriptorSets(vulkan.device, 1, &descriptorWrite, 0, VK_NULL_HANDLE);
    }
    // end create descriptor sets
    // copy data to buffer
    fill_buffer(&vulkan, svBufMem, vertexData, (FillBufferParams){.size = bufferSize});
    copy_buffer(&vulkan, (VulkanCopyBufferParams){.src = svBuffer, .dst = vBuffer, .size = bufferSize});
    fill_buffer(&vulkan, siBufMem, indices, (FillBufferParams){.size = indexSize});
    copy_buffer(&vulkan, (VulkanCopyBufferParams){.src = siBuffer, .dst = iBuffer, .size = indexSize});

    // end copy data to buffer
    // setup command buffers
    update_viewport(&vulkan);
    RecordCmdBuffersParams record_cmd_buffers_args = {
        .vBuffer = vBuffer,
        .iBuffer = iBuffer,
        .indexCount = lengths[index],
        .indexOffset = offsets[index],
        .descriptorSets = descriptorSets};

    uint32_t sizes[] = {800, 600, 900, 540, 512, 512};
    uint32_t sizeIndex = 0, sizesLength = sizeof(sizes) / (sizeof(*sizes) * 2);

    SDL_Event event;
    char running = 1, fullscreen = 0, border = 1;
    uint32_t currentFrame = 0, frameno = 0;
    while (running) {
        while (SDL_PollEvent(&event)) {
            // printf("SDL event %d\n", event.type);
            switch (event.type) {
            default: break;
            case SDL_QUIT: running = 0; break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    recreate_swapchain(&vulkan);
                    reset_command_pool(&vulkan);
                    update_viewport(&vulkan);
                    break;
                }
                break;
            case SDL_KEYDOWN:
                printf(
                    "Keydown scancode %s keycode %s\n", SDL_GetScancodeName(event.key.keysym.scancode),
                    SDL_GetKeyName(event.key.keysym.sym));
                switch (event.key.keysym.scancode) {
                default: break;
                case SDL_SCANCODE_F:
                    sizeIndex = (sizeIndex + 1) % sizesLength;
                    printf("resolution %d %d\n", sizes[2 * sizeIndex + 0], sizes[2 * sizeIndex + 1]);
                    SDL_SetWindowSize(window, sizes[2 * sizeIndex + 0], sizes[2 * sizeIndex + 1]);
                    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    break;
                case SDL_SCANCODE_G:
                    index = (index + 1) % presetLength;
                    printf("index: %d\n", index);
                    recreate_swapchain(&vulkan);
                    reset_command_pool(&vulkan);
                    break;
                case SDL_SCANCODE_B:
                    border = !border;
                    printf("border: %d\n", border);
                    SDL_SetWindowBordered(window, border);
                    break;
                case SDL_SCANCODE_F11:
                    fullscreen = !fullscreen;
                    SDL_SetWindowFullscreen(window, fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    if (!fullscreen) SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    break;
                case SDL_SCANCODE_Q:
                    if (event.key.keysym.mod & KMOD_CTRL) running = 0;
                    break;
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                {
                    char const *button;
                    switch (event.button.button) {
                    case SDL_BUTTON_LEFT: button = "Left"; break;
                    case SDL_BUTTON_MIDDLE: button = "Middle"; break;
                    case SDL_BUTTON_RIGHT: button = "Right"; break;
                    default: button = "Unknown"; break;
                    }
                    printf("%s button down\n", button);
                    break;
                }
            }
            break;
        }
        // draw frame
        vkWaitForFences(vulkan.device, 1, vulkan.frontFences + currentFrame, VK_TRUE, UINT64_MAX);
        uint32_t imageIndex = 0;
        frameno++;
        VkResult res = vkAcquireNextImageKHR(
            vulkan.device, vulkan.swapchain, UINT64_MAX, vulkan.waitSemaphores[currentFrame], VK_NULL_HANDLE,
            &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_swapchain(&vulkan);
            continue;
        }
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            eprintf(MSG_ERROR("vkAcquireNextImageKHR %d"), res);
            break;
        }
        // update uniform buffer
        vec3 axis = {0, 0, 1};
        struct MVP mvp = {
            .model = GLM_MAT4_IDENTITY_INIT,
        };
        glm_rotate(mvp.model, frameno * glm_rad(90.) * 0.001, axis);
        glm_lookat((vec3){2, 2, 2}, (vec3){0, 0, 0}, axis, mvp.view);
        float aspect = vulkan.swcExtent.width / (float)vulkan.swcExtent.height;
        glm_perspective(glm_rad(45), aspect, 0.1, 10, mvp.proj);
        mvp.proj[1][1] *= -1;

        memcpy(uBufsMapped[currentFrame], &mvp, sizeof(mvp));
        // end update uniform buffer

        vkResetFences(vulkan.device, 1, vulkan.frontFences + currentFrame);
        vkResetCommandBuffer(vulkan.commandBuffers[currentFrame], 0);
        record_command_buffer(&vulkan, graphicsPipeline, plLayout, currentFrame, imageIndex, record_cmd_buffers_args);

        VkPipelineStageFlags plStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pNext = VK_NULL_HANDLE,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = vulkan.waitSemaphores + currentFrame,
            .pWaitDstStageMask = &plStage,
            .commandBufferCount = 1,
            .pCommandBuffers = vulkan.commandBuffers + currentFrame,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = vulkan.signalSemaphores + currentFrame};
        vkQueueSubmit(vulkan.drawQueue, 1, &submitInfo, vulkan.frontFences[currentFrame]);

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext = VK_NULL_HANDLE,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = vulkan.signalSemaphores + currentFrame,
            .swapchainCount = 1,
            .pSwapchains = &vulkan.swapchain,
            .pImageIndices = &imageIndex,
            .pResults = VK_NULL_HANDLE};
        vkQueuePresentKHR(vulkan.presentQueue, &presentInfo);
        currentFrame = (currentFrame + 1) % vulkan.maxFrames;
        // end draw frame
    }
    vkDeviceWaitIdle(vulkan.device);

    // vkFreeDescriptorSets(vulkan.device, descriptorPool, vulkan.maxFrames, descriptorSets);
descriptor_sets_failed:
    free(descriptorSets);
    free(layouts);
descriptor_pool_failed:
    vkDestroyDescriptorPool(vulkan.device, descriptorPool, VK_NULL_HANDLE);
    // vkDestroyDescriptorSetLayout(vulkan.device, descriptorSetLayout, VK_NULL_HANDLE);
cleanup:
    // destroy staging buffers
    vkDestroyBuffer(vulkan.device, svBuffer, VK_NULL_HANDLE);
    vkFreeMemory(vulkan.device, svBufMem, VK_NULL_HANDLE);
    vkDestroyBuffer(vulkan.device, siBuffer, VK_NULL_HANDLE);
    vkFreeMemory(vulkan.device, siBufMem, VK_NULL_HANDLE);
    vkFreeMemory(vulkan.device, vBufMem, VK_NULL_HANDLE);
    vkDestroyBuffer(vulkan.device, vBuffer, VK_NULL_HANDLE);
    vkFreeMemory(vulkan.device, iBufMem, VK_NULL_HANDLE);
    vkDestroyBuffer(vulkan.device, iBuffer, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < vulkan.maxFrames; i++) {
        vkUnmapMemory(vulkan.device, uBufMems[i]);
        vkFreeMemory(vulkan.device, uBufMems[i], VK_NULL_HANDLE);
        vkDestroyBuffer(vulkan.device, uBuffers[i], VK_NULL_HANDLE);
    }
    if (graphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(vulkan.device, graphicsPipeline, VK_NULL_HANDLE);
    if (descriptorSetLayout != VK_NULL_HANDLE)
        vkDestroyDescriptorSetLayout(vulkan.device, descriptorSetLayout, VK_NULL_HANDLE);
    if (plLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vulkan.device, plLayout, VK_NULL_HANDLE);
    destroy_vulkan(&vulkan);
    eprintf(MSG_INFO("program finished"));
    return 0;
shader_load_error:
    eprintf(MSG_ERROR("Shader load error"));
    if (!vertShader.size) eprintf(MSG_ERROR("Shader '%s' not loaded"), vertShaderPath);
    if (!fragShader.size) eprintf(MSG_ERROR("Shader '%s' not loaded"), fragShaderPath);
    return 2;
vulkan_init_error:
    eprintf(MSG_ERROR("Vulkan init error: %s"), VulkanInitStatus_str(vulkan.status));
    return 1;
}

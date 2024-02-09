#include "SDL.h"
#include "buffer.h"
#include "command.h"
#include "image.h"
#include "lodepng.h"
#include "my_vulkan.h"
#include "pipeline.h"
#include "shader.h"
#include "utils.h"
#include "vertex.h"
#include "vulkan/vulkan.h"
#include <cglm/cglm.h>
#include <stdio.h>
#include <time.h>

int main(void) {
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
    AShader vertShader =
        AShader_from_path(vulkan.device, vertShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
    AShader fragShader =
        AShader_from_path(vulkan.device, fragShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (vertShader.module == NULL || fragShader.module == NULL) { goto shader_load_error; }
    eprintf(MSG_INFO("Shaders loaded successfully"));
    VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(vulkan.device);
    VkPipelineLayout plLayout =
        create_pipeline_layout(vulkan.device, 1, &descriptorSetLayout, 0, NULL);
    uint32_t uBufBinding = 0; // binding index for uniform buffers in shaders
    APipelineParams plArgs = APipeline_default(uBufBinding);
    VkPipeline graphicsPipeline = create_pipeline(
        vulkan.device, plLayout, vulkan.renderPass, "main", 2, (AShader[]){vertShader, fragShader},
        plArgs);
    // destroy excess
    AShader_destroy(vulkan.device, vertShader);
    AShader_destroy(vulkan.device, fragShader);
    APipelineParams_free(plArgs);
    VkDeviceMemory textureImageMemory;
    VkImage textureImage = create_texture_image(
        vulkan.device, vulkan.pdevice, "data/textures/test.png", vulkan.commandPool,
        vulkan.drawQueue, &textureImageMemory);
    if (textureImage == NULL) {
        eprintf(MSG_ERROR("cannot create image"));
        goto no_texture_image;
    }
    VkImageView textureImageView = create_texture_image_view();
    if (textureImageView == NULL) {
        eprintf(MSG_ERROR("cannot create image view"));
        goto no_texture_image_view;
    }

    struct MVP {
        mat4 model;
        mat4 view;
        mat4 proj;
    };

#define RGB(x) {(x >> 16 & 0xff) / 256., (x >> 8 & 0xff) / 256., (x & 0xff) / 256.}

    Vertex vertexData[] = {
  // 0
        {.pos = {-1., -1.}, .color = RGB(0x7f7f7f)},
        {.pos = {+1., -1.}, .color = RGB(0x0000ff)},
        {.pos = {-1., +1.}, .color = RGB(0xff0000)},
        {.pos = {+1., +1.}, .color = RGB(0x00ff00)},
 // 1
        {.pos = {+1., +1.}, .color = RGB(0xb11132)},
        {.pos = {-1., +1.}, .color = RGB(0xa333f1)},
        {.pos = {+0., +0.}, .color = RGB(0xabcdef)},
        {.pos = {-1., -1.}, .color = RGB(0x422245)},
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
    VkBuffer svBuffer, vBuffer, siBuffer, iBuffer, *uBuffers;
    VkDeviceMemory svBufMem, vBufMem, siBufMem, iBufMem, *uBufsMem;
    void **uBufsMapped;
    VkDeviceSize uBufSize = sizeof(struct MVP);
    svBuffer = create_staging_buffer(vulkan.device, vulkan.pdevice, bufferSize, &svBufMem);
    vBuffer = create_vertex_buffer(vulkan.device, vulkan.pdevice, bufferSize, &vBufMem);
    siBuffer = create_staging_buffer(vulkan.device, vulkan.pdevice, indexSize, &siBufMem);
    iBuffer = create_index_buffer(vulkan.device, vulkan.pdevice, indexSize, &iBufMem);
    uBuffers = create_uniform_buffers(
        vulkan.device, vulkan.pdevice, vulkan.maxFrames, uBufSize, &uBufsMem, &uBufsMapped);
    // end create buffers
    // create descriptor pool
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = vulkan.maxFrames};
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,

        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
        .maxSets = vulkan.maxFrames};
    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(vulkan.device, &poolInfo, NULL, &descriptorPool) != VK_SUCCESS) {
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

        .descriptorPool = descriptorPool,
        .descriptorSetCount = vulkan.maxFrames,
        .pSetLayouts = layouts};
    ARR_ALLOC(VkDescriptorSet, descriptorSets, vulkan.maxFrames);
    if (vkAllocateDescriptorSets(vulkan.device, &dsAInfo, descriptorSets) != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to allocate descriptor sets"));
        goto descriptor_sets_failed;
    }
    // populate descriptors
    for (uint32_t i = 0; i < vulkan.maxFrames; i++) {
        VkDescriptorBufferInfo bufInfo = {
            .buffer = uBuffers[i],
            .offset = 0,
            .range = sizeof(struct MVP) // VK_WHOLE_SIZE
        };
        VkWriteDescriptorSet descriptorWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],
            .dstBinding = uBufBinding,
            .dstArrayElement = 0, // from
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1, // count
            .pBufferInfo = &bufInfo,
            .pImageInfo = NULL;
        // device, descriptor count to write, which to write, count to copy, which to copy
        vkUpdateDescriptorSets(vulkan.device, 1, &descriptorWrite, 0, NULL);
    }
    // end create descriptor sets
    // copy data to buffer
    fill_buffer(vulkan.device, svBufMem, vertexData, (FillBufferParams){.size = bufferSize});
    copy_buffer(
        vulkan.device, vulkan.commandPool, vulkan.drawQueue,
        (ACopyBufferParams){.src = svBuffer, .dst = vBuffer, .size = bufferSize});
    fill_buffer(vulkan.device, siBufMem, indices, (FillBufferParams){.size = indexSize});
    copy_buffer(
        vulkan.device, vulkan.commandPool, vulkan.drawQueue,
        (ACopyBufferParams){.src = siBuffer, .dst = iBuffer, .size = indexSize});

    // end copy data to buffer
    // setup command buffers
    update_viewport(&vulkan);
    ARecordCmdBuffersParams recordArgs = {
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
    float aspect = vulkan.swcExtent.width / (float)vulkan.swcExtent.height;
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
                    aspect = vulkan.swcExtent.width / (float)vulkan.swcExtent.height;
                    break;
                }
                break;
            case SDL_KEYDOWN:
                printf(
                    "Keydown scancode %s keycode %s\n",
                    SDL_GetScancodeName(event.key.keysym.scancode),
                    SDL_GetKeyName(event.key.keysym.sym));
                switch (event.key.keysym.scancode) {
                default: break;
                case SDL_SCANCODE_F:
                    sizeIndex = (sizeIndex + 1) % sizesLength;
                    printf(
                        "resolution %d %d\n", sizes[2 * sizeIndex + 0], sizes[2 * sizeIndex + 1]);
                    SDL_SetWindowSize(window, sizes[2 * sizeIndex + 0], sizes[2 * sizeIndex + 1]);
                    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                    break;
                case SDL_SCANCODE_G:
                    index = (index + 1) % presetLength;
                    printf("index: %d\n", index);
                    recordArgs.indexCount = lengths[index];
                    recordArgs.indexOffset = offsets[index];
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
                    if (!fullscreen)
                        SDL_SetWindowPosition(
                            window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
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
            vulkan.device, vulkan.swapchain, UINT64_MAX, vulkan.waitSemaphores[currentFrame], NULL,
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
        unsigned long long currentTimeNS;
        {
            struct timespec currentTime0;
            clock_gettime(CLOCK_MONOTONIC, &currentTime0);
            currentTimeNS = currentTime0.tv_nsec;
        }
        glm_rotate(mvp.model, currentTimeNS * 2e-9 * glm_rad(90.), axis);
        glm_lookat((vec3){2, 2, 2}, (vec3){0, 0, 0}, axis, mvp.view);
        glm_perspective(glm_rad(45), aspect, 0.1, 10, mvp.proj);
        mvp.proj[1][1] *= -1;

        memcpy(uBufsMapped[currentFrame], &mvp, sizeof(mvp));
        // end update uniform buffer

        vkResetFences(vulkan.device, 1, vulkan.frontFences + currentFrame);
        vkResetCommandBuffer(vulkan.commandBuffers[currentFrame], 0);
        record_command_buffer(
            &vulkan, graphicsPipeline, plLayout, currentFrame, imageIndex, recordArgs);

        VkPipelineStageFlags plStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

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

            .waitSemaphoreCount = 1,
            .pWaitSemaphores = vulkan.signalSemaphores + currentFrame,
            .swapchainCount = 1,
            .pSwapchains = &vulkan.swapchain,
            .pImageIndices = &imageIndex,
            .pResults = NULL};
        vkQueuePresentKHR(vulkan.presentQueue, &presentInfo);
        currentFrame = (currentFrame + 1) % vulkan.maxFrames;
        // end draw frame
    }
    vkDeviceWaitIdle(vulkan.device);

descriptor_sets_failed:
    free(descriptorSets);
    free(layouts);
descriptor_pool_failed:
    vkDestroyDescriptorPool(vulkan.device, descriptorPool, NULL);
    // cleanup
    // destroy staging buffers
    vkDestroyBuffer(vulkan.device, svBuffer, NULL);
    vkFreeMemory(vulkan.device, svBufMem, NULL);
    vkDestroyBuffer(vulkan.device, siBuffer, NULL);
    vkFreeMemory(vulkan.device, siBufMem, NULL);
    vkFreeMemory(vulkan.device, vBufMem, NULL);
    vkDestroyBuffer(vulkan.device, vBuffer, NULL);
    vkFreeMemory(vulkan.device, iBufMem, NULL);
    vkDestroyBuffer(vulkan.device, iBuffer, NULL);
    for (uint32_t i = 0; i < vulkan.maxFrames; i++) {
        vkUnmapMemory(vulkan.device, uBufsMem[i]);
        vkFreeMemory(vulkan.device, uBufsMem[i], NULL);
        vkDestroyBuffer(vulkan.device, uBuffers[i], NULL);
    }
no_texture_image_view:
    vkFreeMemory(vulkan.device, textureImageMemory, NULL);
    vkDestroyImage(vulkan.device, textureImage, NULL);
no_texture_image:
    vkDestroyPipeline(vulkan.device, graphicsPipeline, NULL);
    vkDestroyDescriptorSetLayout(vulkan.device, descriptorSetLayout, NULL);
    vkDestroyPipelineLayout(vulkan.device, plLayout, NULL);
    destroy_vulkan(&vulkan);
    eprintf(MSG_INFO("program finished"));
    return 0;
shader_load_error:
    eprintf(MSG_ERROR("Shader load error"));
    if (vertShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), vertShaderPath);
    if (fragShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), fragShaderPath);
    return 2;
vulkan_init_error:
    eprintf(MSG_ERROR("Vulkan init error: %s"), VulkanInitStatus_str(vulkan.status));
    return 1;
}

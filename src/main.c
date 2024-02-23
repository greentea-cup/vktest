#include "SDL.h"
#include "buffer.h"
#include "command.h"
#include "image.h"
#include "lodepng.h"
#include "my_vulkan.h"
#include "pipeline.h"
#include "shader.h"
#include "sync.h"
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

    // init vulkan
    uint32_t maxFrames = 2;
    VkInstance instance = A_create_instance(window, VK_API_VERSION_1_0);
    VkSurfaceKHR surface = A_create_surface(window, instance);
    VkPhysicalDevice pdevice = A_select_pdevice(instance);
    AQueueFamilies queueFamilies = A_select_queue_families(pdevice, surface);
    if (queueFamilies.graphicsIndex == -1 || queueFamilies.presentIndex == -1) {
        // goto no_queue_families;
        exit(1);
    }
    ADevice adevice = A_create_device(pdevice, queueFamilies);
    VkDevice device = adevice.device;
    if (!A_is_surface_supported(pdevice, queueFamilies.graphicsIndex, surface)) {}
    ASwapchain swapchain = A_create_swapchain(pdevice, surface, queueFamilies, device);
    VkRenderPass renderPass = A_create_render_pass(device, swapchain.imageFormat);
    // descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout = create_descriptor_set_layout(device);
    // graphics pipeline
// shaders
#define SHADER_PATH_PREFIX "./data/shaders_compiled/"
#define SHADER_PATH(x) SHADER_PATH_PREFIX x
    char const *vertShaderPath = SHADER_PATH("main.vert.spv");
    char const *fragShaderPath = SHADER_PATH("main.frag.spv");
    AShader vertShader = AShader_from_path(device, vertShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
    AShader fragShader = AShader_from_path(device, fragShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (vertShader.module == NULL || fragShader.module == NULL) { goto shader_load_error; }
    eprintf(MSG_INFO("Shaders loaded successfully"));
    VkPipelineLayout plLayout = create_pipeline_layout(device, 1, &descriptorSetLayout, 0, NULL);
    uint32_t uBufBinding = 0, samplerBinding = 1; // binding index for uniform buffers in shaders
    APipelineParams plArgs = APipeline_default(uBufBinding);
    VkPipeline graphicsPipeline = create_pipeline(
        device, plLayout, renderPass, "main", 2, (AShader[]){vertShader, fragShader}, plArgs);
    // destroy excess
    AShader_destroy(device, vertShader);
    AShader_destroy(device, fragShader);
    APipelineParams_free(plArgs);

    //
    VkFramebuffer *framebuffers = A_create_framebuffers(device, renderPass, swapchain);
    VkCommandPool commandPool = A_create_command_pool(device, queueFamilies.graphicsIndex);
    // create buffers
    // data
    struct MVP {
        mat4 model;
        mat4 view;
        mat4 proj;
    };

#define RGB(x) {(x >> 16 & 0xff) / 256., (x >> 8 & 0xff) / 256., (x & 0xff) / 256.}

    Vertex vertexData[] = {
  // 0
        {.pos = {-1., -1., 0.},  .color = RGB(0x7f7f7f), .texCoord = {1., 1.}},
        {.pos = {+1., -1., 0.},  .color = RGB(0x0000ff), .texCoord = {1., 0.}},
        {.pos = {-1., +1., 0.},  .color = RGB(0xff0000), .texCoord = {0., 1.}},
        {.pos = {+1., +1., 0.},  .color = RGB(0x00ff00), .texCoord = {0., 0.}},
 // 1
        {.pos = {+1., +1., -.5}, .color = RGB(0xb11132), .texCoord = {0., 0.}},
        {.pos = {-1., +1., -.5}, .color = RGB(0xa333f1), .texCoord = {0., 1.}},
        {.pos = {+0., +0., -.5}, .color = RGB(0xabcdef), .texCoord = {.5, .5}},
        {.pos = {-1., -1., -.5}, .color = RGB(0x422245), .texCoord = {1., 1.}},
        {.pos = {+1., -1., -.5}, .color = RGB(0xa2abf3), .texCoord = {1., 0.}},
 // 2
    };
    VertexIdx indices[] = {
        0, 1, 2, 2, 1, 3,                   // 0 front
        4, 5, 6, 6, 5, 7, 7, 8, 6, 6, 8, 4, // 1 front
    };
    uint32_t offsets[] = {0, 6, 0};
    uint32_t lengths[] = {6, 12, 18};
    uint32_t index = 0;
    uint32_t presetLength = ARR_LEN(offsets);
    uint32_t bufferSize = sizeof(vertexData); // sizeof(*vertexData) * vertexCount;
    uint32_t indexSize = sizeof(indices);
    //
    VkBuffer svBuffer, vBuffer, siBuffer, iBuffer, *uBuffers;
    VkDeviceMemory svBufMem, vBufMem, siBufMem, iBufMem, *uBufsMem;
    void **uBufsMapped;
    VkDeviceSize uBufSize = sizeof(struct MVP);
    // vertex buffer
    svBuffer = create_staging_buffer(device, pdevice, bufferSize, &svBufMem);
    vBuffer = create_vertex_buffer(device, pdevice, bufferSize, &vBufMem);
    // index buffer
    siBuffer = create_staging_buffer(device, pdevice, indexSize, &siBufMem);
    iBuffer = create_index_buffer(device, pdevice, indexSize, &iBufMem);
    // uniform buffers
    uBuffers =
        create_uniform_buffers(device, pdevice, maxFrames, uBufSize, &uBufsMem, &uBufsMapped);
    // end create buffers

    // descriptor pool
    VkDescriptorPoolSize poolSizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         .descriptorCount = maxFrames},
        {.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = maxFrames}
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = ARR_LEN(poolSizes),
        .pPoolSizes = poolSizes,
        .maxSets = maxFrames};
    VkDescriptorPool descriptorPool;
    if (vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool) != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to create descriptor pool"));
        goto descriptor_pool_failed;
    }
    // end descriptor pool
    // descriptor sets
    // layouts is duped descriptorSetLayout
    ARR_ALLOC(VkDescriptorSetLayout, layouts, maxFrames);
    for (uint32_t i = 0; i < maxFrames; i++) { layouts[i] = descriptorSetLayout; }
    VkDescriptorSetAllocateInfo dsAInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = maxFrames,
        .pSetLayouts = layouts};
    ARR_ALLOC(VkDescriptorSet, descriptorSets, maxFrames);
    if (vkAllocateDescriptorSets(device, &dsAInfo, descriptorSets) != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to allocate descriptor sets"));
        goto descriptor_sets_failed;
    }
    // textures
    VkDeviceMemory textureImageMemory;
    VkImage textureImage = create_texture_image(
        device, pdevice, "data/textures/256/test2.png", commandPool, adevice.drawQueue,
        &textureImageMemory);
    if (textureImage == NULL) {
        eprintf(MSG_ERROR("cannot create image"));
        goto no_texture_image;
    }
    VkImageView textureImageView = create_texture_image_view(device, textureImage);
    if (textureImageView == NULL) {
        eprintf(MSG_ERROR("failed to create image view"));
        goto no_texture_image_view;
    }
    VkSampler textureSampler = create_sampler(device);
    if (textureSampler == NULL) {
        eprintf(MSG_ERROR("failed to create texture sampler"));
        goto no_texture_sampler;
    }
    // populate descriptors
    for (uint32_t i = 0; i < maxFrames; i++) {
        VkDescriptorBufferInfo bufInfo = {
            .buffer = uBuffers[i],
            .offset = 0,
            .range = sizeof(struct MVP) // VK_WHOLE_SIZE
        };
        VkDescriptorImageInfo imageInfo = {
            .sampler = textureSampler,
            .imageView = textureImageView,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet descriptorWrites[] = {
            {
             .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = descriptorSets[i],
             .dstBinding = uBufBinding,
             .dstArrayElement = 0, // from
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
             .descriptorCount = 1, // count
                .pBufferInfo = &bufInfo,
             },
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet = descriptorSets[i],
             .dstBinding = samplerBinding,
             .dstArrayElement = 0,
             .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
             .descriptorCount = 1,
             .pImageInfo = &imageInfo}
        };
        // device, descriptor count to write, which to write, count to copy, which to copy
        vkUpdateDescriptorSets(device, ARR_LEN(descriptorWrites), descriptorWrites, 0, NULL);
    }
    // end descriptor sets
    VkCommandBuffer *commandBuffers = A_create_command_buffers(device, commandPool, maxFrames);
    // sync
    VkSemaphore *waitSemaphores = create_semaphores(device, maxFrames);
    VkSemaphore *signalSemaphores = create_semaphores(device, maxFrames);
    VkFence *frontFences = create_fences(device, maxFrames);
    // end sync

    // end init vulkan

    // Vulkan vulkan = init_vulkan(window);
    // if (status != VIS_OK) goto vulkan_init_error;
    eprintf(MSG_INFO("Vulkan initialized successfully"));
    // uint32_t vertexCount = indexSize / sizeof(*indices);
    // copy data to buffer
    fill_buffer(device, svBufMem, vertexData, (FillBufferParams){.size = bufferSize});
    copy_buffer(
        device, commandPool, adevice.drawQueue,
        (ACopyBufferParams){.src = svBuffer, .dst = vBuffer, .size = bufferSize});
    fill_buffer(device, siBufMem, indices, (FillBufferParams){.size = indexSize});
    copy_buffer(
        device, commandPool, adevice.drawQueue,
        (ACopyBufferParams){.src = siBuffer, .dst = iBuffer, .size = indexSize});
    // end copy data to buffer
    // setup command buffers
    VkViewport viewport = make_viewport(swapchain.extent);
    VkRect2D scissor = make_scissor(swapchain.extent, 0, 0, 0, 0);
    ARecordCmdBuffersParams recordArgs = {
        .vBuffer = vBuffer,
        .iBuffer = iBuffer,
        .indexCount = lengths[index],
        .indexOffset = offsets[index],
        .descriptorSets = descriptorSets};

    uint32_t sizes[] = {800, 600, 900, 540, 512, 512};
    uint32_t sizeIndex = 0, sizesLength = sizeof(sizes) / (sizeof(*sizes) * 2);

    SDL_Event event;
    char running = 1, fullscreen = 0, border = 1, timeIncrement = 1;
    uint32_t currentFrame = 0;
    long long gameTimeNS = 0, prevTimeNS = 0;
    float aspect = swapchain.extent.width / (float)swapchain.extent.height;
    while (running) {
        while (SDL_PollEvent(&event)) {
            // printf("SDL event %d\n", event.type);
            switch (event.type) {
            default: break;
            case SDL_QUIT: running = 0; break;
            case SDL_WINDOWEVENT:
                switch (event.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:;
                    ARecreatedSwapchain newSwapchain = A_recreate_swapchain(
                        pdevice, surface, queueFamilies, device, renderPass, swapchain,
                        framebuffers);
                    swapchain = newSwapchain.swapchain;
                    framebuffers = newSwapchain.framebuffers;
                    vkResetCommandPool(device, commandPool, 0);
                    viewport = make_viewport(swapchain.extent);
                    scissor = make_scissor(swapchain.extent, 0, 0, 0, 0);
                    aspect = swapchain.extent.width / (float)swapchain.extent.height;
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
                    ARecreatedSwapchain newSwapchain = A_recreate_swapchain(
                        pdevice, surface, queueFamilies, device, renderPass, swapchain,
                        framebuffers);
                    swapchain = newSwapchain.swapchain;
                    framebuffers = newSwapchain.framebuffers;
                    vkResetCommandPool(device, commandPool, 0);
                    break;
                case SDL_SCANCODE_H:
                    timeIncrement = !timeIncrement;
                    printf("timeIncrement: %d\n", timeIncrement);
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
                    printf("fullscreen: %d\n", fullscreen);
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
        vkWaitForFences(device, 1, frontFences + currentFrame, VK_TRUE, UINT64_MAX);
        uint32_t imageIndex = 0;
        VkResult res = vkAcquireNextImageKHR(
            device, swapchain.swapchain, UINT64_MAX, waitSemaphores[currentFrame], NULL,
            &imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR) {
            ARecreatedSwapchain newSwapchain = A_recreate_swapchain(
                pdevice, surface, queueFamilies, device, renderPass, swapchain, framebuffers);
            swapchain = newSwapchain.swapchain;
            framebuffers = newSwapchain.framebuffers;

            continue;
        }
        if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
            eprintf(MSG_ERROR("vkAcquireNextImageKHR %d"), res);
            break;
        }
        // update uniform buffer
        struct MVP mvp = {
            .model = GLM_MAT4_IDENTITY_INIT,
        };
        {
            long long currentTimeNS;
            struct timespec currentTime0;
            clock_gettime(CLOCK_MONOTONIC, &currentTime0);
            currentTimeNS = currentTime0.tv_nsec;
            if (timeIncrement) gameTimeNS += (currentTimeNS - prevTimeNS);
            prevTimeNS = currentTimeNS;
            // printf("gameTimeNS: %llu\tcurrentTimeNS: %llu\n", gameTimeNS, currentTimeNS);
        }
        vec3 axis = {0, 0, 1}, eye = {2, 2, 2};
        float rotationTime = gameTimeNS * 1e-9;
        glm_rotate(mvp.model, 2 * GLM_PIf * rotationTime, axis);
        glm_lookat(eye, (vec3){0, 0, 0}, axis, mvp.view);
        glm_perspective(glm_rad(45), aspect, 0.1, 10, mvp.proj);
        mvp.proj[1][1] *= -1;

        memcpy(uBufsMapped[currentFrame], &mvp, sizeof(mvp));
        // end update uniform buffer

        vkResetFences(device, 1, frontFences + currentFrame);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);
        record_command_buffer(
            renderPass, framebuffers, swapchain.extent, commandBuffers, viewport, scissor,
            graphicsPipeline, plLayout, currentFrame, imageIndex, recordArgs);

        VkPipelineStageFlags plStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores + currentFrame,
            .pWaitDstStageMask = &plStage,
            .commandBufferCount = 1,
            .pCommandBuffers = commandBuffers + currentFrame,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores + currentFrame};
        vkQueueSubmit(adevice.drawQueue, 1, &submitInfo, frontFences[currentFrame]);

        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores + currentFrame,
            .swapchainCount = 1,
            .pSwapchains = &swapchain.swapchain,
            .pImageIndices = &imageIndex};
        vkQueuePresentKHR(adevice.presentQueue, &presentInfo);
        currentFrame = (currentFrame + 1) % maxFrames;
        // end draw frame
    }
    vkDeviceWaitIdle(device);

descriptor_sets_failed:
    free(descriptorSets);
    free(layouts);
descriptor_pool_failed:
    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    // cleanup
    // destroy staging buffers
    vkDestroyBuffer(device, svBuffer, NULL);
    vkFreeMemory(device, svBufMem, NULL);
    vkDestroyBuffer(device, siBuffer, NULL);
    vkFreeMemory(device, siBufMem, NULL);
    vkFreeMemory(device, vBufMem, NULL);
    vkDestroyBuffer(device, vBuffer, NULL);
    vkFreeMemory(device, iBufMem, NULL);
    vkDestroyBuffer(device, iBuffer, NULL);
    for (uint32_t i = 0; i < maxFrames; i++) {
        vkUnmapMemory(device, uBufsMem[i]);
        vkFreeMemory(device, uBufsMem[i], NULL);
        vkDestroyBuffer(device, uBuffers[i], NULL);
    }
    vkDestroySampler(device, textureSampler, NULL);
no_texture_sampler:
    vkDestroyImageView(device, textureImageView, NULL);
no_texture_image_view:
    vkFreeMemory(device, textureImageMemory, NULL);
    vkDestroyImage(device, textureImage, NULL);
no_texture_image:
    vkDestroyPipeline(device, graphicsPipeline, NULL);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
    vkDestroyPipelineLayout(device, plLayout, NULL);
    // destroy_vulkan(&vulkan);
    eprintf(MSG_INFO("program finished"));
    return 0;
shader_load_error:
    eprintf(MSG_ERROR("Shader load error"));
    if (vertShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), vertShaderPath);
    if (fragShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), fragShaderPath);
    return 2;
    // vulkan_init_error:
    // eprintf(MSG_ERROR("Vulkan init error: %s"), VulkanInitStatus_str(status));
    return 1;
}

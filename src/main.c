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
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        eprintf(MSG_ERROR("cannot init sdl: %s"), SDL_GetError());
        goto no_sdl;
    }
    // create SDL window
    SDL_Window *window = SDL_CreateWindow(
        "vktest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == NULL) {
        eprintf(MSG_ERROR("cannot create window: %s"), SDL_GetError());
        goto no_window;
    }
    // init vulkan
    uint32_t maxFrames = 2;
    VkInstance instance = A_create_instance(window, VK_API_VERSION_1_0);
    if (instance == NULL) {
        eprintf(MSG_ERROR("cannot create vulkan instance"));
        goto no_instance;
    }
    VkSurfaceKHR surface = A_create_surface(window, instance);
    if (surface == NULL) {
        eprintf(MSG_ERROR("cannot create surface"));
        goto no_surface;
    }
    VkPhysicalDevice pdevice = A_select_pdevice(instance);
    AQueueFamilies queueFamilies = A_select_queue_families(pdevice, surface);
    if (queueFamilies.graphicsIndex == -1 || queueFamilies.presentIndex == -1) {
        eprintf(MSG_ERROR("cannot find queue families"));
        goto no_queue_families;
    }
    ADevice adevice = ADevice_create(pdevice, queueFamilies);
    if (adevice.device == NULL) {
        eprintf(MSG_ERROR("cannot create vulkan device"));
        goto no_device;
    }
    VkDevice device = adevice.device;
    if (!A_is_surface_supported(pdevice, queueFamilies.graphicsIndex, surface)) {
        eprintf(MSG_ERROR("surface is not supported by selected physical device"));
        goto no_surface_support;
    }
    ASwapchain swapchain = ASwapchain_create(pdevice, surface, queueFamilies, device);
    if (swapchain.swapchain == NULL) {
        eprintf(MSG_ERROR("cannot create swapchain"));
        goto no_swapchain;
    }
    VkRenderPass renderPass = A_create_render_pass(device, swapchain.imageFormat);
    if (renderPass == NULL) {
        eprintf(MSG_ERROR("cannot create render pass"));
        goto no_render_pass;
    }
    // descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout = A_create_descriptor_set_layout(device);
    if (descriptorSetLayout == NULL) {
        eprintf(MSG_ERROR("cannot create descriptor set layout"));
        goto no_descriptor_set_layout;
    }
    // graphics pipeline
    VkPipelineLayout plLayout = A_create_pipeline_layout(device, 1, &descriptorSetLayout, 0, NULL);
    if (plLayout == NULL) {
        eprintf(MSG_ERROR("cannot create pipeline layout"));
        goto no_pipeline_layout;
    }
    uint32_t uBufBinding = 0, samplerBinding = 1; // binding index for uniform buffers in shaders
// shaders
#define SHADER_PATH_PREFIX "./data/shaders_compiled/"
#define SHADER_PATH(x) SHADER_PATH_PREFIX x
    char const *vertShaderPath = SHADER_PATH("main.vert.spv");
    char const *fragShaderPath = SHADER_PATH("main.frag.spv");
    AShader vertShader = AShader_from_path(device, vertShaderPath, VK_SHADER_STAGE_VERTEX_BIT);
    AShader fragShader = AShader_from_path(device, fragShaderPath, VK_SHADER_STAGE_FRAGMENT_BIT);
    if (vertShader.module == NULL || fragShader.module == NULL) {
        eprintf(MSG_ERROR("cannot load shaders"));
        if (vertShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), vertShaderPath);
        else AShader_destroy(device, vertShader);
        if (fragShader.module == NULL) eprintf(MSG_ERROR("Shader '%s' not loaded"), fragShaderPath);
        else AShader_destroy(device, fragShader);
        goto no_shaders;
    }
    eprintf(MSG_INFO("Shaders loaded successfully"));
    APipelineParams plArgs = APipeline_default(uBufBinding);
    VkPipeline graphicsPipeline = A_create_pipeline(
        device, plLayout, renderPass, "main", 2, (AShader[]){vertShader, fragShader}, plArgs);
    // destroy excess
    AShader_destroy(device, vertShader);
    AShader_destroy(device, fragShader);
    APipelineParams_free(plArgs);
    //
    if (graphicsPipeline == NULL) {
        eprintf(MSG_ERROR("cannot create pipeline"));
        goto no_pipeline;
    }
    //
    VkFramebuffer *framebuffers =
        A_create_framebuffers(device, renderPass, swapchain); // count = swapchain.imageCount
    if (framebuffers == NULL) {
        eprintf(MSG_ERROR("cannot create framebuffers"));
        goto no_framebuffers;
    }
    VkCommandPool commandPool = A_create_command_pool(device, queueFamilies.graphicsIndex);
    if (commandPool == NULL) {
        eprintf(MSG_ERROR("cannot create command pool"));
        goto no_command_pool;
    }
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
    VkBuffer svBuffer, vBuffer, siBuffer, iBuffer;
    VkDeviceMemory svBufMem, vBufMem, siBufMem, iBufMem;
    VkBuffer *uBuffers;       // count = maxFrames
    VkDeviceMemory *uBufsMem; // count = maxFrames
    void **uBufsMapped;       // count = maxFrames
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
    if (svBuffer == NULL || vBuffer == NULL || siBuffer == NULL || iBuffer == NULL ||
        uBuffers == NULL) {
        eprintf(MSG_ERROR("cannot create buffers"));
        goto partial_buffers;
    }
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
        goto no_descriptor_pool;
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
    VkResult res = vkAllocateDescriptorSets(device, &dsAInfo, descriptorSets);
    free(layouts);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("failed to allocate descriptor sets"));
        goto no_descriptor_sets;
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
    if (commandBuffers == NULL) {
        eprintf(MSG_ERROR("cannot create command buffers"));
        goto no_command_buffers;
    }
    // sync
    VkSemaphore *waitSemaphores = create_semaphores(device, maxFrames);
    VkSemaphore *signalSemaphores = create_semaphores(device, maxFrames);
    VkFence *frontFences = create_fences(device, maxFrames);
    // TODO: return NULL from upper calls
    if (waitSemaphores == NULL || signalSemaphores == NULL || frontFences == NULL) {
        eprintf("cannot create synchronization objects");
        goto no_sync;
    }
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
    char running = 1, fullscreen = 0, border = 1, timeIncrement = 1, rotationIncrement = 1;
    uint32_t currentFrame = 0;
    double gameTime = 0, dayTime = 0, rotationTime = 0, prevTime, timeSpeed = 1, rotationSpeed = 1,
           rotationPeriod = 2 * GLM_PI, dayLength = 24 * 60 * 60;
    {
        struct timespec prevTime0;
        timespec_get(&prevTime0, TIME_UTC);
        prevTime = prevTime0.tv_sec + prevTime0.tv_nsec * 1e-9;
    }
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
                case SDL_SCANCODE_J:
                    if (event.key.keysym.mod & KMOD_SHIFT) timeSpeed /= 2;
                    else timeSpeed *= 2;
                    printf("timeSpeed: %f\n", timeSpeed);
                    break;
                case SDL_SCANCODE_K:
                    if (event.key.keysym.mod & KMOD_SHIFT) rotationSpeed /= 2;
                    else rotationSpeed *= 2;
                    printf("rotationSpeed: %f\n", rotationSpeed);
                    break;
                case SDL_SCANCODE_L:
                    rotationIncrement = !rotationIncrement;
                    printf("rotationIncrement: %d\n", rotationIncrement);
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
            struct timespec currentTime0;
            timespec_get(&currentTime0, TIME_UTC);
            double currentTime = currentTime0.tv_sec + currentTime0.tv_nsec * 1e-9;

            if (timeIncrement) {
                double timeDelta = (currentTime - prevTime) * timeSpeed;
                gameTime += timeDelta;
                dayTime += timeDelta;
                dayTime -= dayLength * floor(dayTime / dayLength);
                if (rotationIncrement) {
                    rotationTime += timeDelta * rotationSpeed;
                    rotationTime -= rotationPeriod * floor(rotationTime / rotationPeriod);
                }
                /* printf(
                    "gameTime: %lf dayTime: %lf rotationTime: %lf\n", gameTime, dayTime,
                    rotationTime);*/
            }
            prevTime = currentTime;
        }
        vec3 axis = {0, 0, 1}, eye = {2, 2, 2};
        glm_rotate(mvp.model, 2 * GLM_PI * rotationTime, axis);
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

    // unwind start

    for (uint32_t i = 0; i < maxFrames; i++) {
        vkDestroyFence(device, frontFences[i], NULL);
        vkDestroySemaphore(device, signalSemaphores[i], NULL);
        vkDestroySemaphore(device, waitSemaphores[i], NULL);
    }
no_sync:
    // commandBuffers[maxFrames]
    vkFreeCommandBuffers(device, commandPool, maxFrames, commandBuffers);
    free(commandBuffers);
no_command_buffers:
    // textureSampler
    vkDestroySampler(device, textureSampler, NULL);
no_texture_sampler:
    // textueImageView
    vkDestroyImageView(device, textureImageView, NULL);
no_texture_image_view:
    // textureImageMemory
    // textureImage
    vkFreeMemory(device, textureImageMemory, NULL);
    vkDestroyImage(device, textureImage, NULL);
no_texture_image:
    // descriptorSets[maxFrames]
    // vkFreeDescriptorSets is not aplicable
    // because descriptor pool's FREE flag is not set
    free(descriptorSets);
no_descriptor_sets:
    // descriptorPool
    vkDestroyDescriptorPool(device, descriptorPool, NULL);
no_descriptor_pool:
partial_buffers:
    // uBufsMapped[maxFrames]
    // uBufsMem[maxFrames], iBufMem, siBufMem, svBufMem, vBufMem
    // svBuffer, vBuffer, siBuffer, iBuffer, uBuffers[maxFrames]
    free(uBufsMapped); // NULL ok
    // NOTE: vkFreeMemory implies vkUnmapMemory if it was mapped
    if (uBufsMem != NULL && uBuffers != NULL)
        for (uint32_t i = 0; i < maxFrames; i++) {
            // inside are non-null handles
            vkFreeMemory(device, uBufsMem[i], NULL);
            vkDestroyBuffer(device, uBuffers[i], NULL);
        }
    if (iBufMem != NULL) vkFreeMemory(device, iBufMem, NULL);
    if (siBufMem != NULL) vkFreeMemory(device, siBufMem, NULL);
    if (vBufMem != NULL) vkFreeMemory(device, vBufMem, NULL);
    if (svBufMem != NULL) vkFreeMemory(device, svBufMem, NULL);
    if (iBuffer != NULL) vkDestroyBuffer(device, iBuffer, NULL);
    if (siBuffer != NULL) vkDestroyBuffer(device, siBuffer, NULL);
    if (vBuffer != NULL) vkDestroyBuffer(device, vBuffer, NULL);
    if (svBuffer != NULL) vkDestroyBuffer(device, svBuffer, NULL);
    // no_buffers: // (unused)
    // commandPool
    vkDestroyCommandPool(device, commandPool, NULL);
no_command_pool:
    // framebuffers[swapchain.imageCount]
    for (uint32_t i = 0; i < swapchain.imageCount; i++) {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }
no_framebuffers:
    // graphicsPipeline
    vkDestroyPipeline(device, graphicsPipeline, NULL);
no_pipeline:
    // empty
no_shaders:
    // plLayout
    vkDestroyPipelineLayout(device, plLayout, NULL);
no_pipeline_layout:
    // descriptorSetLayout
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
no_descriptor_set_layout:
    // renderPass
    vkDestroyRenderPass(device, renderPass, NULL);
no_render_pass:
    // swapchain
    ASwapchain_destroy(device, swapchain);
no_swapchain:
    // empty
no_surface_support:
    // adevice
    // device{adevice}
    device = NULL;
    ADevice_destroy(adevice);
no_device:
    // empty
no_queue_families:
    // surface
    vkDestroySurfaceKHR(instance, surface, NULL);
no_surface:
    // instance
    vkDestroyInstance(instance, NULL);
no_instance:
    // window
    SDL_DestroyWindow(window);
no_window:
    // empty
no_sdl:
    // empty
    return 0;
}

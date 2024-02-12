#include "my_vulkan.h"
#include "SDL_vulkan.h"
#include "command.h"
#include "image.h"
#include "pipeline.h"
#include "shader.h"
#include "sync.h"
#include "utils.h"

/*
 * 0 on success
 * 1 if cannot create instance
 */
static int create_vulkan_instance(Vulkan *vulkan, SDL_Window *window) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = NULL,
        .applicationVersion = 0,
        .pEngineName = NULL,
        .engineVersion = 0,
        .apiVersion = VK_API_VERSION_1_0};
#ifndef NDEBUG
    // # apt install vulkan-validationlayers
    char const *ivkLayers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = 1;
    eprintf(MSG_INFO("Running in DEBUG, adding validation layers"));
#else
    char const *const *ivkLayers = NULL;
    uint32_t layerCount = 0;
#endif
    uint32_t ivkExtCount;
    SDL_Vulkan_GetInstanceExtensions(window, &ivkExtCount, NULL);
    ARR_ALLOC(char const *, ivkExtensions, ivkExtCount);
    SDL_Vulkan_GetInstanceExtensions(window, &ivkExtCount, ivkExtensions);
    VkInstanceCreateInfo ivkCInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = ivkLayers,
        .enabledExtensionCount = ivkExtCount,
        .ppEnabledExtensionNames = ivkExtensions};
    VkInstance ivk;
    VkResult res = vkCreateInstance(&ivkCInfo, NULL, &ivk);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create vulkan instance: %d"), res);
        vulkan->status = VIS_CANNOT_CREATE_INSTANCE;
        return 1;
    }
    vulkan->ivk = ivk;
    return 0;
}

/*
 * 0 on success
 * 1 if cannot create surface
 */
static int create_surface(Vulkan *vulkan, SDL_Window *window) {
    SDL_bool surfaceCreated = SDL_Vulkan_CreateSurface(window, vulkan->ivk, &vulkan->surface);
    if (!surfaceCreated) {
        vulkan->status = VIS_CANNOT_CREATE_SURFACE;
        return 1;
    }
    return 0;
}

/*
 * returns provided physical device suitablility score
 */
static int calculate_physical_device_suitability(VkPhysicalDevice pdevice) {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(pdevice, &props);
    vkGetPhysicalDeviceFeatures(pdevice, &features);

    if (!features.geometryShader || !features.samplerAnisotropy) return 0;
    int score = 0;
    switch (props.deviceType) {
    default: break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 1000; break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 500; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 250; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: score += 100; break;
    }

    return score;
}

/*
 * 0 on success
 * 0 if best device is bad, status flag is set
 * to VIS_BAD_PHYSICAL_DEVICE
 */
static int select_physical_device(Vulkan *vulkan) {
    uint32_t pDevCount;
    vkEnumeratePhysicalDevices(vulkan->ivk, &pDevCount, NULL);
    ARR_ALLOC(VkPhysicalDevice, devices, pDevCount);
    vkEnumeratePhysicalDevices(vulkan->ivk, &pDevCount, devices);
    uint32_t bestDeviceIndex = 0;
    int bestDeviceScore = 0;
    for (uint32_t i = 0; i < pDevCount; i++) {
        int thisScore = calculate_physical_device_suitability(devices[i]);
        if (thisScore > bestDeviceScore) {
            bestDeviceScore = thisScore;
            bestDeviceIndex = i;
        }
    }
    if (bestDeviceScore == 0) {
        eprintf(
            MSG_ERROR("Environment doesn't meet minimum requirements. Stability not guaranteed."));
        // do not return error; maybe it still works
        vulkan->status = VIS_BAD_PHYSICAL_DEVICE;
    }
    vulkan->pdevice = devices[bestDeviceIndex];
    free(devices);
    return 0;
}

/*
 * 0 on success
 * 1 if bad queue family, status flag is set
 * to VIS_BAD_QUEUE_FAMILY
 */
static int select_queue_families(Vulkan *vulkan, uint32_t *out_qFamCount) {
    uint32_t qFamCount;
    vkGetPhysicalDeviceQueueFamilyProperties(
        vulkan->pdevice, &qFamCount, NULL); // get array size into qFamCount
    ARR_ALLOC(VkQueueFamilyProperties, qFamProps, qFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(vulkan->pdevice, &qFamCount, qFamProps);
    // find graphics family
    int32_t gIdx = -1, pIdx = -1;
    for (uint32_t i = 0; i < qFamCount; i++) {
        VkQueueFamilyProperties qFam = qFamProps[i];
        if (qFam.queueCount > 0 && qFam.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            // select best == highest count
            if (gIdx == -1 || qFamProps[gIdx].queueCount < qFam.queueCount) gIdx = i;
        }
        VkBool32 presentSupport = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(vulkan->pdevice, i, vulkan->surface, &presentSupport);
        if (presentSupport) pIdx = i;
        if (gIdx != -1 && pIdx != -1) break;
    }
    free(qFamProps);
    if (gIdx == -1 || pIdx == -1) {
        eprintf(MSG_ERROR("cannot find suitable queue families"));
        vulkan->status = VIS_BAD_QUEUE_FAMILY;
        return 1;
    }
    *out_qFamCount = qFamCount;
    vulkan->graphicsQFI = gIdx;
    vulkan->presentQFI = pIdx;
    return 0;
}

/*
 * 0 on success
 * 1 if cannot create device, status flag is set
 * to VIS_CANNOT_CREATE_DEVICE
 */
static int create_device(Vulkan *vulkan) {
    uint32_t qFamCount;
    if (select_queue_families(vulkan, &qFamCount)) {
        eprintf(MSG_ERROR("create_device failed: select_queue_families failed"));
        return 1;
    }
    // device queue create info && queue priorities
    ARR_ALLOC(VkDeviceQueueCreateInfo, devQCInfo, qFamCount);
    float qPriority = 1.0f;
    for (uint32_t i = 0; i < qFamCount; i++) {
        devQCInfo[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &qPriority};
    }

    // physical device extensions
    char const *extensions[VK_MAX_EXTENSION_NAME_SIZE] = {"VK_KHR_swapchain"};
    uint32_t extensionCount = 1;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(vulkan->pdevice, &features);
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo devCInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = qFamCount,
        .pQueueCreateInfos = devQCInfo,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &features};
    VkResult res = vkCreateDevice(vulkan->pdevice, &devCInfo, NULL, &vulkan->device);
    // cleanup
    free(devQCInfo);
    if (res != VK_SUCCESS) {
        eprintf("device creation failed");
        vulkan->status = VIS_CANNOT_CREATE_DEVICE;
        return 1;
    }
    // get queues
    vkGetDeviceQueue(vulkan->device, vulkan->graphicsQFI, 0, &vulkan->drawQueue);
    vkGetDeviceQueue(vulkan->device, vulkan->presentQFI, 0, &vulkan->presentQueue);
    return 0;
}

/*
 * 0 on success
 * 1 if surface is not supported, status flag is set
 * to VIS_SURFACE_NOT_SUPPORTED
 */
static int check_surface_support(Vulkan *vulkan) {
    VkBool32 isSurfaceSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        vulkan->pdevice, vulkan->graphicsQFI, vulkan->surface, &isSurfaceSupported);
    if (!isSurfaceSupported) {
        vulkan->status = VIS_SURFACE_NOT_SUPPORTED;
        return 1;
    }
    return 0;
}

static VkSurfaceFormatKHR get_surface_format(VkPhysicalDevice pdevice, VkSurfaceKHR surface) {
    uint32_t surfFmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &surfFmtCount, NULL);
    ARR_ALLOC(VkSurfaceFormatKHR, surfFormats, surfFmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &surfFmtCount, surfFormats);
    VkSurfaceFormatKHR surfFormat = surfFormats[0]; //
    free(surfFormats);
    return surfFormat;
}

static VkPresentModeKHR get_present_mode(VkPhysicalDevice pdevice, VkSurfaceKHR surface) {
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &presentModeCount, NULL);
    ARR_ALLOC(VkPresentModeKHR, presentModes, presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &presentModeCount, presentModes);
    VkPresentModeKHR thePresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            thePresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
            break;
        }
    }
    free(presentModes);
    return thePresentMode;
}

/*
 * 0 on success
 * 1 if cannot create swapchain, status flag is set
 * to VIS_CANNOT_CREATE_SWAPCHAIN
 */
static int create_swapchain(Vulkan *vulkan) {
    VkSurfaceCapabilitiesKHR surfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->pdevice, vulkan->surface, &surfCaps);
    VkSurfaceFormatKHR surfFormat = get_surface_format(vulkan->pdevice, vulkan->surface);
    VkPresentModeKHR presentMode = get_present_mode(vulkan->pdevice, vulkan->surface);
    // swapchain extent
    vulkan->swcExtent = surfCaps.currentExtent;
    // create swapchain
    VkSharingMode imageShareMode = VK_SHARING_MODE_EXCLUSIVE;
    uint32_t qFamIdxCount = 0;
    uint32_t const *qFamIndices = NULL;
    uint32_t const qfids[] = {vulkan->graphicsQFI, vulkan->presentQFI};
    if (qfids[0] != qfids[1]) { // different graphics and present queues
        imageShareMode = VK_SHARING_MODE_CONCURRENT;
        qFamIdxCount = 2;
        qFamIndices = qfids;
    }
    uint32_t imageCount = surfCaps.minImageCount + 1;
    uint32_t maxImageCount = surfCaps.maxImageCount;
    if (maxImageCount != 0 && imageCount > maxImageCount) imageCount = maxImageCount;
    VkSwapchainCreateInfoKHR swcCInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vulkan->surface,
        .minImageCount = surfCaps.minImageCount + 1,
        .imageFormat = surfFormat.format,
        .imageColorSpace = surfFormat.colorSpace,
        .imageExtent = vulkan->swcExtent,
        .imageArrayLayers = 1, // see VkSwapchainCreateInfoKHR Khronos registry
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VK_IMAGE_USAGE_TRANSFER_DST_BIT when
                                                           // get to post-processing
        .imageSharingMode = imageShareMode,
        .queueFamilyIndexCount = qFamIdxCount,
        .pQueueFamilyIndices = qFamIndices,
        .preTransform = surfCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL};
    VkResult res = vkCreateSwapchainKHR(vulkan->device, &swcCInfo, NULL, &vulkan->swapchain);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create swapchain: %d"), res);
        vulkan->status = VIS_CANNOT_CREATE_SWAPCHAIN;
        return 1;
    }
    vkGetSwapchainImagesKHR(vulkan->device, vulkan->swapchain, &vulkan->swcImageCount, NULL);
    vulkan->swcImages = ARR_INPLACE_ALLOC(VkImage, vulkan->swcImageCount);
    vkGetSwapchainImagesKHR(
        vulkan->device, vulkan->swapchain, &vulkan->swcImageCount, vulkan->swcImages);

    vulkan->swcImageFormat = surfFormat.format;
    return 0;
}

/*
 * 0 on success
 * 1 if cannot create image view
 */
static int create_image_views(Vulkan *vulkan) {
    vulkan->swcImageViews = ARR_INPLACE_ALLOC(VkImageView, vulkan->swcImageCount);
    for (uint32_t i = 0; i < vulkan->swcImageCount; i++) {
        VkImageView swcImageView =
            create_image_view(vulkan->device, vulkan->swcImages[i], vulkan->swcImageFormat);
        if (swcImageView == NULL) {
            eprintf(MSG_ERROR("failed create image view"));
            return 1;
        }
        vulkan->swcImageViews[i] = swcImageView;
    }
    return 0;
}

/*
 * 0 on success
 * 1 if cannot create render pass, status flag is set
 * to VIS_CANNOT_CREATE_RENDER_PASS
 */
static int create_render_pass(Vulkan *vulkan) {
    VkAttachmentDescription attachDesc = {
        .format = vulkan->swcImageFormat,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

    VkAttachmentReference attachRef = {
        .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpassDesc = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachRef,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = NULL,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL};

    VkSubpassDependency subpassDep = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0};
    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &attachDesc,
        .subpassCount = 1,
        .pSubpasses = &subpassDesc,
        .dependencyCount = 1,
        .pDependencies = &subpassDep};

    VkResult res = vkCreateRenderPass(vulkan->device, &renderPassInfo, NULL, &vulkan->renderPass);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create render pass: %d"), res);
        vulkan->status = VIS_CANNOT_CREATE_RENDER_PASS;
        return 1;
    }
    return 0;
}

/*
 * 0 on success
 * 1 if cannot create framebuffer, status flag is set
 * to VIS_CANNOT_CREATE_FRAMEBUFFER
 */
static int create_framebuffers(Vulkan *vulkan) {
    VkFramebufferCreateInfo fbCInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = vulkan->renderPass,
        .attachmentCount = 1,
        .pAttachments = NULL,
        .width = vulkan->swcExtent.width,
        .height = vulkan->swcExtent.height,
        .layers = 1};
    vulkan->framebuffers = ARR_INPLACE_ALLOC(VkFramebuffer, vulkan->swcImageCount);
    for (uint32_t i = 0; i < vulkan->swcImageCount; i++) {
        fbCInfo.pAttachments = vulkan->swcImageViews + i;
        VkResult res =
            vkCreateFramebuffer(vulkan->device, &fbCInfo, NULL, vulkan->framebuffers + i);
        if (res != VK_SUCCESS) {
            eprintf("cannot create framebuffer #%d: %d", i, res);
            vulkan->status = VIS_CANNOT_CREATE_FRAMEBUFFER;
            return 1;
        }
    }
    return 0;
}

char const *VulkanInitStatus_str(VulkanInitStatus status) {
    // idk why clang-format does this
#define CASE_TO_STR(x)                                                                             \
    case x: return #x;
    switch (status) {
        CASE_TO_STR(VIS_OK);
        CASE_TO_STR(VIS_CANNOT_CREATE_INSTANCE);
        CASE_TO_STR(VIS_CANNOT_CREATE_SURFACE);
        CASE_TO_STR(VIS_BAD_PHYSICAL_DEVICE);
        CASE_TO_STR(VIS_BAD_QUEUE_FAMILY);
        CASE_TO_STR(VIS_CANNOT_CREATE_DEVICE);
        CASE_TO_STR(VIS_SURFACE_NOT_SUPPORTED);
        CASE_TO_STR(VIS_CANNOT_CREATE_SWAPCHAIN);
        CASE_TO_STR(VIS_CANNOT_CREATE_RENDER_PASS);
        CASE_TO_STR(VIS_CANNOT_CREATE_FRAMEBUFFER);
    default: return "<VIS_UNKNOWN>";
    }

#undef CASE_TO_STR
}

Vulkan init_vulkan(SDL_Window *window) {
    Vulkan vulkan = {0};
    vulkan.status = VIS_OK;
    vulkan.maxFrames = 2;
    if (create_vulkan_instance(&vulkan, window)) goto error;
    if (create_surface(&vulkan, window)) goto error;
    if (select_physical_device(&vulkan)) goto error;
    if (create_device(&vulkan)) goto error;
    if (check_surface_support(&vulkan)) goto error;
    if (create_swapchain(&vulkan)) goto error;
    if (create_image_views(&vulkan)) goto error;
    if (create_render_pass(&vulkan)) goto error;
    // descriptor set layout
    // graphics pipeline
    if (create_framebuffers(&vulkan)) goto error;
    vulkan.commandPool = create_command_pool(vulkan.device, vulkan.graphicsQFI);
    // vertex buffer
    // index buffer
    // uniform buffers
    // descriptor pool
    // descriptor sets
    vulkan.commandBuffers =
        create_command_buffers(vulkan.device, vulkan.commandPool, vulkan.maxFrames);
    // sync vvv
    vulkan.waitSemaphores = create_semaphores(vulkan.device, vulkan.maxFrames);
    vulkan.signalSemaphores = create_semaphores(vulkan.device, vulkan.maxFrames);
    vulkan.frontFences = create_fences(vulkan.device, vulkan.maxFrames);
    // end sync

    return vulkan;
error:
    // error == return struct uninitialized but provide status code
    // clean up
    destroy_vulkan(&vulkan);
    return vulkan;
}

static void cleanup_swapchain(Vulkan *vulkan) {
    for (uint32_t i = 0; i < vulkan->swcImageCount; i++)
        vkDestroyFramebuffer(vulkan->device, vulkan->framebuffers[i], NULL);
    for (uint32_t i = 0; i < vulkan->swcImageCount; i++)
        vkDestroyImageView(vulkan->device, vulkan->swcImageViews[i], NULL);
    vkDestroySwapchainKHR(vulkan->device, vulkan->swapchain, NULL);
    free(vulkan->framebuffers);
    free(vulkan->swcImageViews);
    free(vulkan->swcImages);
}

void destroy_vulkan(Vulkan *vulkan) {
    if (vulkan->device == NULL) goto nodevice;
    for (uint32_t i = 0; i < vulkan->maxFrames; i++) {
        vkDestroyFence(vulkan->device, vulkan->frontFences[i], NULL);
        vkDestroySemaphore(vulkan->device, vulkan->signalSemaphores[i], NULL);
        vkDestroySemaphore(vulkan->device, vulkan->waitSemaphores[i], NULL);
    }
    if (vulkan->swcImageCount != 0) {
        vkFreeCommandBuffers(
            vulkan->device, vulkan->commandPool, vulkan->maxFrames, vulkan->commandBuffers);
        vkDestroyCommandPool(vulkan->device, vulkan->commandPool, NULL);
    }
    if (vulkan->renderPass != NULL) vkDestroyRenderPass(vulkan->device, vulkan->renderPass, NULL);
    cleanup_swapchain(vulkan);
    vkDestroyDevice(vulkan->device, NULL);
nodevice:
    // regular free at the end for convenience
    // free(vulkan->backFences);
    free(vulkan->frontFences);
    free(vulkan->signalSemaphores);
    free(vulkan->waitSemaphores);
    free(vulkan->commandBuffers);
    if (vulkan->surface != NULL) vkDestroySurfaceKHR(vulkan->ivk, vulkan->surface, NULL);
    vkDestroyInstance(vulkan->ivk, NULL);
}

/*
 * 0 on success
 * 1 if cannot recreate swapchain
 */
int recreate_swapchain(Vulkan *vulkan) {
    vkDeviceWaitIdle(vulkan->device);
    cleanup_swapchain(vulkan);
    if (create_swapchain(vulkan)) goto error;
    if (create_image_views(vulkan)) goto error;
    if (create_framebuffers(vulkan)) goto error;
    return 0;
error:
    eprintf(MSG_ERROR("cannot recreate swapchain"));
    return 1;
}

void reset_command_pool(Vulkan *vulkan) {
    vkResetCommandPool(vulkan->device, vulkan->commandPool, 0);
}

void update_viewport(Vulkan *vulkan) {
    vulkan->viewport = make_viewport(vulkan->swcExtent);
    vulkan->scissor = make_scissor(vulkan->swcExtent, 0, 0, 0, 0);
}

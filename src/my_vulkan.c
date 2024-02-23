#include "my_vulkan.h"
#include "SDL_vulkan.h"
#include "command.h"
#include "image.h"
#include "pipeline.h"
#include "shader.h"
#include "sync.h"
#include "utils.h"

static VkInstance create_vulkan_instance(
    SDL_Window *window, VkApplicationInfo appInfo, uint32_t layerCount, char const *const *layers) {
    uint32_t extensionCount;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
    ARR_ALLOC(char const *, extensions, extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions);
    VkInstanceCreateInfo instanceInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layers,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions};
    VkInstance instance;
    VkResult res = vkCreateInstance(&instanceInfo, NULL, &instance);
    free(extensions);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create vulkan instance: %d"), res);
        return NULL;
    }
    return instance;
}

VkInstance A_create_instance(SDL_Window *window, uint32_t apiVersion) {
#ifndef NDEBUG
    // # apt install vulkan-validationlayers
    char const *layers[] = {"VK_LAYER_KHRONOS_validation"};
    uint32_t layerCount = 1;
    eprintf(MSG_INFO("Running in DEBUG, adding validation layers"));
#else
    char const *const *layers = NULL;
    uint32_t layerCount = 0;
#endif
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = apiVersion};
    return create_vulkan_instance(window, appInfo, layerCount, layers);
}

VkSurfaceKHR A_create_surface(SDL_Window *window, VkInstance instance) {
    VkSurfaceKHR surface;
    SDL_bool surfaceCreated = SDL_Vulkan_CreateSurface(window, instance, &surface);
    if (!surfaceCreated) {
        eprintf(MSG_ERROR("cannot create surface"));
        return NULL;
    }
    return surface;
}

int A_pdevice_score(VkPhysicalDevice pdevice) {
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

VkPhysicalDevice A_select_pdevice(VkInstance instance) {
    uint32_t pdeviceCount;
    vkEnumeratePhysicalDevices(instance, &pdeviceCount, NULL);
    ARR_ALLOC(VkPhysicalDevice, pdevices, pdeviceCount);
    vkEnumeratePhysicalDevices(instance, &pdeviceCount, pdevices);
    uint32_t bestDeviceIndex = 0;
    int bestDeviceScore = 0;
    for (uint32_t i = 0; i < pdeviceCount; i++) {
        int thisScore = A_pdevice_score(pdevices[i]);
        if (thisScore > bestDeviceScore) {
            bestDeviceScore = thisScore;
            bestDeviceIndex = i;
        }
    }
    VkPhysicalDevice pdevice = pdevices[bestDeviceIndex];
    free(pdevices);
#ifndef A_NO_COMPAT_WARN
    if (bestDeviceScore == 0)
        eprintf(MSG_WARN("Environment doesn't meet minimum requirements. "
                         "Stability not guaranteed.\n"
                         "To disable this warning, define "
                         "A_NO_COMPAT_WARN before compilation"));
#endif
    return pdevice;
}

AQueueFamilies A_select_queue_families(VkPhysicalDevice pdevice, VkSurfaceKHR surface) {
    uint32_t qFamCount;
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &qFamCount, NULL);
    ARR_ALLOC(VkQueueFamilyProperties, qFamProps, qFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &qFamCount, qFamProps);
    // find graphics family
    int32_t gIdx = -1, pIdx = -1;
    for (uint32_t i = 0; i < qFamCount; i++) {
        VkQueueFamilyProperties qFam = qFamProps[i];
        if (qFam.queueCount > 0 && qFam.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            // select best == highest count
            if (gIdx == -1 || qFamProps[gIdx].queueCount < qFam.queueCount) gIdx = i;
        }
        VkBool32 presentSupport = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, surface, &presentSupport);
        if (presentSupport) pIdx = i;
        if (gIdx != -1 && pIdx != -1) break;
    }
    free(qFamProps);
    if (gIdx == -1 || pIdx == -1) { eprintff(MSG_ERRORF("cannot find suitable queue families")); }
    return (AQueueFamilies){.count = qFamCount, .graphicsIndex = gIdx, .presentIndex = pIdx};
}

ADevice A_create_device(VkPhysicalDevice pdevice, AQueueFamilies queueFamilies) {
    // device queue create info && queue priorities
    ARR_ALLOC(VkDeviceQueueCreateInfo, deviceQueueInfos, queueFamilies.count);
    float priority = 1.0f;
    for (uint32_t i = 0; i < queueFamilies.count; i++) {
        deviceQueueInfos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = i,
            .queueCount = 1,
            .pQueuePriorities = &priority};
    }

    // physical device extensions
    char const *extensions[VK_MAX_EXTENSION_NAME_SIZE] = {"VK_KHR_swapchain"};
    uint32_t extensionCount = 1;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(pdevice, &features);
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo deviceInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queueFamilies.count,
        .pQueueCreateInfos = deviceQueueInfos,
        .enabledExtensionCount = extensionCount,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &features};
    VkDevice device;
    VkResult res = vkCreateDevice(pdevice, &deviceInfo, NULL, &device);
    // cleanup
    free(deviceQueueInfos);
    if (res != VK_SUCCESS) {
        eprintff(MSG_ERRORF("device creation failed: %d"), res);
        goto no_device;
    }
    // get queues
    VkQueue drawQueue, presentQueue;
    vkGetDeviceQueue(device, queueFamilies.graphicsIndex, 0, &drawQueue);
    vkGetDeviceQueue(device, queueFamilies.presentIndex, 0, &presentQueue);

    return (ADevice){.device = device, .drawQueue = drawQueue, .presentQueue = presentQueue};
no_device:
    return (ADevice){.device = NULL};
}

VkBool32 A_is_surface_supported(
    VkPhysicalDevice pdevice, uint32_t graphicsFamilyIndex, VkSurfaceKHR surface) {
    VkBool32 isSurfaceSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        pdevice, graphicsFamilyIndex, surface, &isSurfaceSupported);
    return isSurfaceSupported;
}

static VkSurfaceFormatKHR get_surface_format(VkPhysicalDevice pdevice, VkSurfaceKHR surface) {
    uint32_t surfFmtCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &surfFmtCount, NULL);
    ARR_ALLOC(VkSurfaceFormatKHR, surfFormats, surfFmtCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &surfFmtCount, surfFormats);
    VkSurfaceFormatKHR surfFormat = surfFormats[0]; // best compatible
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

ASwapchain A_create_swapchain(
    VkPhysicalDevice pdevice, VkSurfaceKHR surface, AQueueFamilies queueFamilies, VkDevice device) {
    VkSurfaceCapabilitiesKHR surfCaps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &surfCaps);
    VkSurfaceFormatKHR surfFormat = get_surface_format(pdevice, surface);
    VkPresentModeKHR presentMode = get_present_mode(pdevice, surface);
    VkExtent2D swapchainExtent = surfCaps.currentExtent;
    VkSharingMode imageSharingMode;
    uint32_t qFamIdxCount;
    uint32_t const *qFamIndices;
    uint32_t const qfids[] = {queueFamilies.graphicsIndex, queueFamilies.presentIndex};
    if (qfids[0] == qfids[1]) {
        imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        qFamIdxCount = 0;
        qFamIndices = NULL;
    }
    else {
        imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        qFamIdxCount = 2;
        qFamIndices = qfids;
    }
    uint32_t imageCount = surfCaps.minImageCount + 1, maxImageCount = surfCaps.maxImageCount;
    if (maxImageCount != 0 && imageCount > maxImageCount) imageCount = maxImageCount;
    VkFormat swapchainImageFormat = surfFormat.format;
    VkSwapchainCreateInfoKHR swapchainInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = surfCaps.minImageCount + 1,
        .imageFormat = swapchainImageFormat,
        .imageColorSpace = surfFormat.colorSpace,
        .imageExtent = swapchainExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // VK_IMAGE_USAGE_TRANSFER_DST_BIT when
                                                           // get to post-processing
        .imageSharingMode = imageSharingMode,
        .queueFamilyIndexCount = qFamIdxCount,
        .pQueueFamilyIndices = qFamIndices,
        .preTransform = surfCaps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = NULL};
    VkSwapchainKHR swapchain;
    VkResult res = vkCreateSwapchainKHR(device, &swapchainInfo, NULL, &swapchain);
    if (res != VK_SUCCESS) {
        eprintf(MSG_ERROR("cannot create swapchain: %d"), res);
        goto no_swapchain;
    }
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, NULL);
    ARR_ALLOC(VkImage, swapchainImages, swapchainImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages);
    ARR_ALLOC(VkImageView, swapchainImageViews, swapchainImageCount);
    uint32_t imageViewSuccessful;
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        VkImageView imageView = create_image_view(device, swapchainImages[i], swapchainImageFormat);
        if (imageView == NULL) {
            eprintf(MSG_ERROR("failed to create image view"));
            imageViewSuccessful = i; // excluding this
            goto no_image_view;
        }
        swapchainImageViews[i] = imageView;
    }
    return (ASwapchain){
        .swapchain = swapchain,
        .extent = swapchainExtent,
        .imageFormat = swapchainImageFormat,
        .imageCount = swapchainImageCount,
        .images = swapchainImages,
        .imageViews = swapchainImageViews};
no_image_view:
    for (uint32_t i = 0; i < imageViewSuccessful; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }
    free(swapchainImageViews);
    free(swapchainImages);
    vkDestroySwapchainKHR(device, swapchain, NULL);
no_swapchain:
    return (ASwapchain){.swapchain = NULL};
}

VkImageView *A_create_swapchain_image_views(VkDevice device, ASwapchain swapchain) {
    ARR_ALLOC(VkImageView, swapchainImageViews, swapchain.imageCount);
    uint32_t imageViewSuccessful;
    for (uint32_t i = 0; i < swapchain.imageCount; i++) {
        VkImageView imageView =
            create_image_view(device, swapchain.images[i], swapchain.imageFormat);
        if (imageView == NULL) {
            eprintf(MSG_ERROR("failed to create image view"));
            imageViewSuccessful = i; // excluding this
            goto image_view_failed;
        }
        swapchainImageViews[i] = imageView;
    }
    return swapchainImageViews;
image_view_failed:
    for (uint32_t i = 0; i < imageViewSuccessful; i++) {
        vkDestroyImageView(device, swapchainImageViews[i], NULL);
    }
    free(swapchainImageViews);
    return NULL;
}

VkRenderPass A_create_render_pass(VkDevice device, VkFormat imageFormat) {
    VkAttachmentDescription attachDesc = {
        .format = imageFormat,
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
    VkRenderPass renderPass;
    VkResult res = vkCreateRenderPass(device, &renderPassInfo, NULL, &renderPass);
    if (res != VK_SUCCESS) {
        eprintff(MSG_ERRORF("cannot create render pass: %d"), res);
        return NULL;
    }
    return renderPass;
}

VkFramebuffer *A_create_framebuffers(
    VkDevice device, VkRenderPass renderPass, ASwapchain swapchain) {
    VkFramebufferCreateInfo framebufferInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = renderPass,
        .attachmentCount = 1,
        .pAttachments = NULL,
        .width = swapchain.extent.width,
        .height = swapchain.extent.height,
        .layers = 1};
    ARR_ALLOC(VkFramebuffer, framebuffers, swapchain.imageCount);
    uint32_t successful;
    for (uint32_t i = 0; i < swapchain.imageCount; i++) {
        framebufferInfo.pAttachments = swapchain.imageViews + i;
        VkResult res = vkCreateFramebuffer(device, &framebufferInfo, NULL, framebuffers + i);
        if (res != VK_SUCCESS) {
            eprintf("cannot create framebuffer #%d: %d", i, res);
            successful = i;
            goto partial_framebuffers;
        }
    }
    return framebuffers;
partial_framebuffers:
    for (uint32_t i = 0; i < successful; i++) {
        vkDestroyFramebuffer(device, framebuffers[i], NULL);
    }
    free(framebuffers);
    return NULL;
}

/*
void init_vulkan(SDL_Window *window, uint32_t apiVersion) {
    VkInstance instance = A_create_instance(window, apiVersion);
    VkSurfaceKHR surface = A_create_surface(window, instance);
    VkPhysicalDevice pdevice = A_select_pdevice(instance);
    AQueueFamilies queueFamilies = A_select_queue_families(pdevice, surface);
    if (queueFamilies.graphicsIndex == -1 || queueFamilies.presentIndex == -1) {
        eprintff(MSG_ERRORF("no queue families"));
    }
    ADevice device = A_create_device(pdevice, queueFamilies);
    VkBool32 isSurfaceSupported;
    vkGetPhysicalDeviceSurfaceSupportKHR(
        pdevice, queueFamilies.graphicsIndex, surface, &isSurfaceSupported);
    if (!isSurfaceSupported) {}
    ASwapchain swapchain = A_create_swapchain(pdevice, surface, queueFamilies, device);
    VkRenderPass renderPass = A_create_render_pass(device.device, swapchain.imageFormat);
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
*/

/*
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
}*/

void ASwapchain_destroy(VkDevice device, ASwapchain swapchain) {
    // images from vkGetSwapchainImagesKHR
    // should not be destroyed by vkDestroyImage
    for (uint32_t i = 0; i < swapchain.imageCount; i++) {
        vkDestroyImageView(device, swapchain.imageViews[i], NULL);
    }
    vkDestroySwapchainKHR(device, swapchain.swapchain, NULL);
    free(swapchain.imageViews);
    free(swapchain.images);
}

ARecreatedSwapchain A_recreate_swapchain(
    VkPhysicalDevice pdevice, VkSurfaceKHR surface, AQueueFamilies queueFamilies, VkDevice device,
    VkRenderPass renderPass, ASwapchain oldSwapchain,
    VkFramebuffer oldFramebuffers[oldSwapchain.imageCount]) {
    vkDeviceWaitIdle(device);
    for (uint32_t i = 0; i < oldSwapchain.imageCount; i++) {
        vkDestroyFramebuffer(device, oldFramebuffers[i], NULL);
    }
    ASwapchain_destroy(device, oldSwapchain);
    ASwapchain newSwapchain = A_create_swapchain(pdevice, surface, queueFamilies, device);
    VkFramebuffer *newFramebuffers = A_create_framebuffers(device, renderPass, newSwapchain);
    return (ARecreatedSwapchain){.swapchain = newSwapchain, .framebuffers = newFramebuffers};
    // TODO: errors
}

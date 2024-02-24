#ifndef MY_VULKAN_H
#define MY_VULKAN_H
#include "SDL_vulkan.h"
#include "vulkan/vulkan.h"

/*
 * returns VkInstance on success
 * NULL on failure
 */
VkInstance A_create_instance(SDL_Window *window, uint32_t apiVersion);

/*
 */
VkSurfaceKHR A_create_surface(SDL_Window *window, VkInstance instance);

/*
 * returns provided physical device suitablility score
 */
int A_pdevice_score(VkPhysicalDevice pdevice);

/*
 * returns best physical device available
 * if best device is not good enough,
 * reports with eprintf
 */
VkPhysicalDevice A_select_pdevice(VkInstance instance);

typedef struct AQueueFamilies {
    uint32_t count;
    int32_t graphicsIndex;
    int32_t presentIndex;
} AQueueFamilies;

/*
 * AQueueFamilies with valid indices on success
 * .graphicsIndex=-1 if no graphics queue
 * .presentIndex=-1 if no present queue
 */
AQueueFamilies A_select_queue_families(VkPhysicalDevice pdevice, VkSurfaceKHR surface);

typedef struct ADevice {
    VkDevice device;
    VkQueue drawQueue;
    VkQueue presentQueue;
} ADevice;

/*
 * .device=NULL on fail
 */
ADevice ADevice_create(VkPhysicalDevice pdevice, AQueueFamilies queueFamilies);

void ADevice_destroy(ADevice adevice);

VkBool32 A_is_surface_supported(
    VkPhysicalDevice pdevice, uint32_t graphicsFamilyIndex, VkSurfaceKHR surface);

typedef struct ASwapchain {
    VkSwapchainKHR swapchain;
    VkExtent2D extent;
    VkFormat imageFormat;
    uint32_t imageCount;
    VkImage *images;         // count = imageCount
    VkImageView *imageViews; // count = imageCount
} ASwapchain;

/*
 */
ASwapchain ASwapchain_create(
    VkPhysicalDevice pdevice, VkSurfaceKHR surface, AQueueFamilies queueFamilies, VkDevice device);

void ASwapchain_destroy(VkDevice device, ASwapchain swapchain);

/*
 */
VkImageView *A_create_swapchain_image_views(VkDevice device, ASwapchain swapchain);

/*
 */
VkRenderPass A_create_render_pass(VkDevice device, VkFormat imageFormat);

/*
 */
VkFramebuffer *A_create_framebuffers(
    VkDevice device, VkRenderPass renderPass, ASwapchain swapchain);

typedef struct ARecreatedSwapchain {
    ASwapchain swapchain;
    VkFramebuffer *framebuffers; // count = swapchain.imageCount
} ARecreatedSwapchain;

ARecreatedSwapchain A_recreate_swapchain(
    VkPhysicalDevice pdevice, VkSurfaceKHR surface, AQueueFamilies queueFamilies, VkDevice device,
    VkRenderPass renderPass, ASwapchain oldSwapchain,
    VkFramebuffer oldFramebuffers[oldSwapchain.imageCount]);

#endif

// SPDX-License-Identifier: GPL-2.0-or-later

#define VK_USE_PLATFORM_XLIB_KHR
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <X11/Xlib.h>
#include <unistd.h>
#include <vulkan/vulkan.h>
#define OK(x)                                                                                      \
    do {                                                                                           \
        auto r = (x);                                                                              \
        if (r != VK_SUCCESS) {                                                                     \
            fprintf(stderr, "line %d result %d\n", __LINE__, r);                                   \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static unsigned errors = 0;
static VKAPI_ATTR VkBool32 VKAPI_CALL debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                            VkDebugUtilsMessageTypeFlagsEXT,
                                            const VkDebugUtilsMessengerCallbackDataEXT* d, void*) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        fprintf(stderr, "%s\n", d->pMessage);
        ++errors;
    }
    return VK_FALSE;
}
int main(int argc, char** argv) {
    const char* mode = argc > 2 ? argv[2] : "sparse-timeline";
    bool binary = !strcmp(mode, "sparse-binary");
    bool submit_control = !strcmp(mode, "submit-timeline");
    bool negative = !strcmp(mode, "missing-acquire");
    bool host_wait = argc > 1 && !strcmp(argv[1], "wait");
    bool counter = argc > 1 && !strcmp(argv[1], "counter");
    Display* display = XOpenDisplay(nullptr);
    if (!display)
        return 2;
    Window window =
        XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, 1280, 720, 0, 0, 0);
    XMapWindow(display, window);
    XFlush(display);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.apiVersion = VK_API_VERSION_1_4;
    const char* extensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
                                VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    const char* layer = "VK_LAYER_KHRONOS_validation";
    VkValidationFeatureEnableEXT enabled =
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    VkValidationFeaturesEXT validation{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    validation.enabledValidationFeatureCount = 1;
    validation.pEnabledValidationFeatures = &enabled;
    VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = 3;
    ici.ppEnabledExtensionNames = extensions;
    ici.enabledLayerCount = 1;
    ici.ppEnabledLayerNames = &layer;
    ici.pNext = &validation;
    VkInstance instance;
    OK(vkCreateInstance(&ici, nullptr, &instance));
    VkDebugUtilsMessengerCreateInfoEXT dbg{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    dbg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    dbg.messageType = 7;
    dbg.pfnUserCallback = debug;
    VkDebugUtilsMessengerEXT messenger;
    OK(((PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkCreateDebugUtilsMessengerEXT"))(instance, &dbg, nullptr, &messenger));
    VkXlibSurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
    sci.dpy = display;
    sci.window = window;
    VkSurfaceKHR surface;
    OK(vkCreateXlibSurfaceKHR(instance, &sci, nullptr, &surface));
    uint32_t count = 0;
    OK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
    std::vector<VkPhysicalDevice> devices(count);
    OK(vkEnumeratePhysicalDevices(instance, &count, devices.data()));
    VkPhysicalDevice gpu = devices[0];
    uint32_t family = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, families.data());
    for (; family < count; ++family) {
        VkBool32 supports;
        OK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, family, surface, &supports));
        if (supports && (families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            break;
    }
    float priority = 1;
    VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &priority;
    VkPhysicalDeviceTimelineSemaphoreFeatures timeline_feature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES};
    timeline_feature.timelineSemaphore = VK_TRUE;
    VkPhysicalDeviceDynamicRenderingFeatures dynamic{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    dynamic.dynamicRendering = VK_TRUE;
    timeline_feature.pNext = &dynamic;
    VkPhysicalDeviceSynchronization2Features sync2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES};
    sync2.synchronization2 = VK_TRUE;
    dynamic.pNext = &sync2;
    const char* swap_ext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &timeline_feature;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = 1;
    dci.ppEnabledExtensionNames = &swap_ext;
    VkPhysicalDeviceFeatures features{};
    features.sparseBinding = VK_TRUE;
    dci.pEnabledFeatures = &features;
    VkDevice device;
    OK(vkCreateDevice(gpu, &dci, nullptr, &device));
    VkQueue queue;
    vkGetDeviceQueue(device, family, 0, &queue);
    VkSurfaceCapabilitiesKHR caps;
    OK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps));
    OK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(count);
    OK(vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data()));
    for (unsigned i = 0; i < formats.size(); ++i)
        if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            auto f = formats[i];
            formats[0] = f;
            break;
        }
    VkSwapchainCreateInfoKHR swci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swci.surface = surface;
    swci.minImageCount = caps.minImageCount + 1;
    swci.imageFormat = formats[0].format;
    swci.imageColorSpace = formats[0].colorSpace;
    swci.imageExtent = caps.currentExtent;
    swci.imageArrayLayers = 1;
    swci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swci.queueFamilyIndexCount = 1;
    swci.pQueueFamilyIndices = &family;
    swci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swci.preTransform = caps.currentTransform;
    swci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swci.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
    swci.clipped = VK_TRUE;
    VkSwapchainKHR swapchain;
    OK(vkCreateSwapchainKHR(device, &swci, nullptr, &swapchain));
    OK(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr));
    std::vector<VkImage> images(count);
    OK(vkGetSwapchainImagesKHR(device, swapchain, &count, images.data()));
    std::vector<VkImageView> views(count);
    for (unsigned i = 0; i < count; ++i) {
        VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = formats[0].format;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        OK(vkCreateImageView(device, &vi, nullptr, &views[i]));
    }
    VkSemaphoreCreateInfo semci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    std::vector<VkSemaphore> acquired(count), present(count);
    for (unsigned i = 0; i < count; ++i) {
        OK(vkCreateSemaphore(device, &semci, nullptr, &acquired[i]));
        OK(vkCreateSemaphore(device, &semci, nullptr, &present[i]));
    }
    VkSemaphore sparse_ready;
    if (binary)
        OK(vkCreateSemaphore(device, &semci, nullptr, &sparse_ready));
    VkSemaphoreTypeCreateInfo type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO};
    type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    semci.pNext = &type;
    if (!binary)
        OK(vkCreateSemaphore(device, &semci, nullptr, &sparse_ready));
    VkSemaphore ready, done;
    OK(vkCreateSemaphore(device, &semci, nullptr, &ready));
    done = ready;
    VkSemaphore other;
    OK(vkCreateSemaphore(device, &semci, nullptr, &other));
    VkCommandPoolCreateInfo poolci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolci.queueFamilyIndex = family;
    poolci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool;
    OK(vkCreateCommandPool(device, &poolci, nullptr, &pool));
    VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    alloc.commandPool = pool;
    alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc.commandBufferCount = count;
    std::vector<VkCommandBuffer> commands(count);
    OK(vkAllocateCommandBuffers(device, &alloc, commands.data()));
    alloc.commandBufferCount = 1;
    VkCommandBuffer prep_cmd;
    OK(vkAllocateCommandBuffers(device, &alloc, &prep_cmd));
    VkCommandBufferBeginInfo prep_begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    prep_begin.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    OK(vkBeginCommandBuffer(prep_cmd, &prep_begin));
    VkImageCreateInfo imci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imci.imageType = VK_IMAGE_TYPE_2D;
    imci.format = VK_FORMAT_R8G8B8A8_UNORM;
    imci.extent = {16, 16, 1};
    imci.mipLevels = imci.arrayLayers = 1;
    imci.samples = VK_SAMPLE_COUNT_1_BIT;
    imci.tiling = VK_IMAGE_TILING_OPTIMAL;
    imci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    VkImage offscreen;
    OK(vkCreateImage(device, &imci, nullptr, &offscreen));
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(device, offscreen, &mr);
    VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    mai.allocationSize = mr.size;
    while (!(mr.memoryTypeBits & (1u << mai.memoryTypeIndex)))
        ++mai.memoryTypeIndex;
    VkDeviceMemory mem;
    OK(vkAllocateMemory(device, &mai, nullptr, &mem));
    OK(vkBindImageMemory(device, offscreen, mem, 0));
    VkImageMemoryBarrier offbar{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    offbar.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    offbar.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    offbar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    offbar.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    offbar.srcQueueFamilyIndex = offbar.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    offbar.image = offscreen;
    offbar.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(prep_cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &offbar);
    VkClearColorValue offcolor{{0, 0, 0, 1}};
    vkCmdClearColorImage(prep_cmd, offscreen, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &offcolor, 1,
                         &offbar.subresourceRange);
    OK(vkEndCommandBuffer(prep_cmd));
    std::vector<VkFence> fences(count);
    VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (auto& fence : fences)
        OK(vkCreateFence(device, &fci, nullptr, &fence));
    for (uint64_t tick = 1; tick <= (negative ? 6 : 40); ++tick) {
        usleep(16667);
        unsigned frame = (tick - 1) % count;
        OK(vkWaitForFences(device, 1, &fences[frame], VK_TRUE, UINT64_MAX));
        OK(vkResetFences(device, 1, &fences[frame]));
        VkTimelineSemaphoreSubmitInfo tsi{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        tsi.signalSemaphoreValueCount = 1;
        uint64_t prepare_tick = 2 * tick - 1, present_tick = 2 * tick;
        tsi.pSignalSemaphoreValues = &prepare_tick;
        VkSubmitInfo prep{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        prep.pNext = &tsi;
        prep.commandBufferCount = 1;
        prep.pCommandBuffers = &prep_cmd;
        prep.signalSemaphoreCount = 1;
        prep.pSignalSemaphores = &ready;
        OK(vkQueueSubmit(queue, 1, &prep, VK_NULL_HANDLE));
        if (counter) {
            uint64_t value;
            OK(vkGetSemaphoreCounterValue(device, ready, &value));
        }
        unsigned index;
        OK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, acquired[frame], VK_NULL_HANDLE,
                                 &index));
        auto cmd = commands[frame];
        OK(vkResetCommandBuffer(cmd, 0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        OK(vkBeginCommandBuffer(cmd, &begin));
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images[index];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, VK_REMAINING_ARRAY_LAYERS};
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkClearColorValue color{{0.1f, 0.2f, 0.3f, 1.f}};
        VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        att.imageView = views[index];
        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att.clearValue.color = color;
        VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
        rendering.renderArea.extent = caps.currentExtent;
        rendering.layerCount = 1;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachments = &att;
        vkCmdBeginRendering(cmd, &rendering);
        vkCmdEndRendering(cmd);
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                             VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0,
                             nullptr, 0, nullptr, 1, &barrier);
        OK(vkEndCommandBuffer(cmd));
        VkSemaphore waits[] = {acquired[frame], ready}, signals[] = {present[index], done};
        uint64_t wait_values[] = {1, prepare_tick}, signal_values[] = {1, present_tick};
        VkPipelineStageFlags stages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
        tsi.waitSemaphoreValueCount = 2;
        tsi.pWaitSemaphoreValues = wait_values;
        tsi.signalSemaphoreValueCount = 2;
        tsi.pSignalSemaphoreValues = signal_values;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.pNext = &tsi;
        submit.waitSemaphoreCount = 2;
        submit.pWaitSemaphores = waits;
        submit.pWaitDstStageMask = stages;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        submit.signalSemaphoreCount = 2;
        submit.pSignalSemaphores = signals;
        if (negative) {
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &waits[1];
            tsi.waitSemaphoreValueCount = 1;
            tsi.pWaitSemaphoreValues = &wait_values[1];
        }
        OK(vkQueueSubmit(queue, 1, &submit, fences[frame]));
        if (host_wait) {
            VkSemaphoreWaitInfo wi{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
            wi.semaphoreCount = 1;
            wi.pSemaphores = &done;
            wi.pValues = &present_tick;
            OK(vkWaitSemaphores(device, &wi, UINT64_MAX));
        }
        if (counter) {
            uint64_t value;
            OK(vkGetSemaphoreCounterValue(device, done, &value));
        }
        VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &present[index];
        pi.swapchainCount = 1;
        pi.pSwapchains = &swapchain;
        pi.pImageIndices = &index;
        OK(vkQueuePresentKHR(queue, &pi));
        VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
        VkTimelineSemaphoreSubmitInfo sparse_time{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
        sparse_time.signalSemaphoreValueCount = 1;
        sparse_time.pSignalSemaphoreValues = &tick;
        bind.pNext = &sparse_time;
        bind.signalSemaphoreCount = 1;
        bind.pSignalSemaphores = &sparse_ready;
        if (submit_control || negative) {
            VkSubmitInfo substitute{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            substitute.pNext = &sparse_time;
            substitute.signalSemaphoreCount = 1;
            substitute.pSignalSemaphores = &sparse_ready;
            OK(vkQueueSubmit(queue, 1, &substitute, VK_NULL_HANDLE));
        } else {
            OK(vkQueueBindSparse(queue, 1, &bind, VK_NULL_HANDLE));
        }
        for (uint64_t j = 0; j < 8; ++j) {
            uint64_t value = 8 * (tick - 1) + j + 1;
            VkTimelineSemaphoreSubmitInfo ot{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO};
            ot.signalSemaphoreValueCount = 1;
            ot.pSignalSemaphoreValues = &value;
            VkSubmitInfo os{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            os.pNext = &ot;
            os.commandBufferCount = 1;
            os.pCommandBuffers = &prep_cmd;
            VkPipelineStageFlags ws = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            os.waitSemaphoreCount = j == 0 ? 1 : 0;
            os.pWaitSemaphores = &sparse_ready;
            os.pWaitDstStageMask = &ws;
            ot.waitSemaphoreValueCount = os.waitSemaphoreCount;
            ot.pWaitSemaphoreValues = &tick;
            os.signalSemaphoreCount = 1;
            os.pSignalSemaphores = &other;
            OK(vkQueueSubmit(queue, 1, &os, VK_NULL_HANDLE));
            VkSemaphoreWaitInfo ow{VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO};
            ow.semaphoreCount = 1;
            ow.pSemaphores = &other;
            ow.pValues = &value;
            OK(vkWaitSemaphores(device, &ow, UINT64_MAX));
        }
    }
    OK(vkDeviceWaitIdle(device));
    fprintf(stderr, "RESULT frames=%u warnings_errors=%u mode=%s host=%s\n", negative ? 6 : 40,
            errors, mode, argc > 1 ? argv[1] : "normal");
    for (auto f : fences)
        vkDestroyFence(device, f, nullptr);
    for (auto s : acquired)
        vkDestroySemaphore(device, s, nullptr);
    for (auto s : present)
        vkDestroySemaphore(device, s, nullptr);
    vkDestroySemaphore(device, ready, nullptr);
    vkDestroySemaphore(device, other, nullptr);
    vkDestroySemaphore(device, sparse_ready, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    vkDestroyImage(device, offscreen, nullptr);
    vkFreeMemory(device, mem, nullptr);
    for (auto v : views)
        vkDestroyImageView(device, v, nullptr);
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    ((PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT"))(instance, messenger, nullptr);
    vkDestroyInstance(instance, nullptr);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

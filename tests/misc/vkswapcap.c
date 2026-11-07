/* vkswapcap.c: native Linux check for swapchurn's cap (no Wine, no DXVK): how many live VkSwapchainKHR, each on its
 * own XCB window, one VkDevice, before vkCreateSwapchainKHR fails. Under Wine + DXVK the 256th fails with
 * VK_ERROR_INITIALIZATION_FAILED (Audition's long-session hang).
 * Build: cc -O2 -o vkswapcap vkswapcap.c -lvulkan -lxcb -lxcb-shape      Usage: DISPLAY=:1 DEV=<gpu index> ./vkswapcap [count, default 300]
 * ORPHAN=1 presents, destroys the window and keeps the swapchain (DXVK then lost the device);
 * ORPHAN=2 reparents it to an unmapped dummy window instead, as Wine does when the HWND is destroyed
 * DROP=1 destroys each swapchain and keeps its surface: is the cap on surfaces or on swapchains? */
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan.h>
#include <xcb/xcb.h>
#include <xcb/shape.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 300;
    const char *iext[] = {"VK_KHR_surface", "VK_KHR_xcb_surface"}, *dext[] = {"VK_KHR_swapchain"};
    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_1};
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app,
                                .enabledExtensionCount = 2, .ppEnabledExtensionNames = iext};
    VkInstance inst; VkPhysicalDevice pd[4]; uint32_t npd = 4; VkDevice dev; float prio = 1;
    if (vkCreateInstance(&ici, NULL, &inst)) { puts("instance failed"); return 1; }
    vkEnumeratePhysicalDevices(inst, &npd, pd);
    int k = getenv("DEV") ? atoi(getenv("DEV")) : 0; pd[0] = pd[k < (int)npd ? k : 0];
    VkPhysicalDeviceProperties pp; vkGetPhysicalDeviceProperties(pd[0], &pp); printf("device: %s\n", pp.deviceName);
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex = 0, .queueCount = 1,
                                   .pQueuePriorities = &prio};
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount = 1, .pQueueCreateInfos = &qci,
                              .enabledExtensionCount = 1, .ppEnabledExtensionNames = dext};
    if (vkCreateDevice(pd[0], &dci, NULL, &dev)) { puts("device failed"); return 1; }
    xcb_connection_t *c = xcb_connect(NULL, NULL);
    xcb_screen_t *scr = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    for (int i = 0; i < n; i++)
    {
        xcb_window_t w = xcb_generate_id(c);
        xcb_create_window(c, XCB_COPY_FROM_PARENT, w, scr->root, 40, 40, 451, 410, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          scr->root_visual, 0, NULL);
        xcb_map_window(c, w); xcb_flush(c);
        VkXcbSurfaceCreateInfoKHR sci = {VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR, .connection = c, .window = w};
        VkSurfaceKHR s; VkResult r = vkCreateXcbSurfaceKHR(inst, &sci, NULL, &s);
        if (r) { printf("surface %d: %d\n", i, r); return 1; }
        VkBool32 ok; vkGetPhysicalDeviceSurfaceSupportKHR(pd[0], 0, s, &ok);
        VkSurfaceCapabilitiesKHR cap; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd[0], s, &cap);
        VkSwapchainCreateInfoKHR sw = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR, .surface = s,
            .minImageCount = cap.minImageCount < 3 ? 3 : cap.minImageCount, .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR, .imageExtent = {451, 410}, .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, .preTransform = cap.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, .presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR, .clipped = 1};
        VkSwapchainKHR sc; r = vkCreateSwapchainKHR(dev, &sw, NULL, &sc);
        if (r) { printf("swapchain %d: VkResult %d (supported %u)\n", i, r, ok); return 2; }
        if (getenv("DROP")) vkDestroySwapchainKHR(dev, sc, NULL);   /* DROP=1: keep only the surfaces live */
        if (getenv("ORPHAN"))   /* ORPHAN=1: present 3 frames, then destroy the X window under the live swapchain */
        {
            VkQueue q; vkGetDeviceQueue(dev, 0, 0, &q);
            for (int f = 0; f < 3; f++)
            {
                VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; VkFence fe; uint32_t idx;
                vkCreateFence(dev, &fci, NULL, &fe);
                r = vkAcquireNextImageKHR(dev, sc, UINT64_MAX, VK_NULL_HANDLE, fe, &idx);
                if (r < 0) { printf("acquire %d: %d\n", i, r); return 3; }
                vkWaitForFences(dev, 1, &fe, 1, UINT64_MAX); vkDestroyFence(dev, fe, NULL);
                VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, .swapchainCount = 1, .pSwapchains = &sc,
                                       .pImageIndices = &idx};
                r = vkQueuePresentKHR(q, &pi);
                if (r < 0) { printf("present %d: %d\n", i, r); return 3; }
            }
            if (getenv("IDLE")) vkQueueWaitIdle(q);   /* IDLE=1: no present in flight when the window goes */
            if (atoi(getenv("ORPHAN")) == 2)   /* what Wine does: reparent to an unmapped dummy window */
            {
                static xcb_window_t dummy;
                if (!dummy)
                {
                    dummy = xcb_generate_id(c);
                    uint32_t ovr = 1;   /* MAPPED=1: override-redirect and mapped, like Wine's get_dummy_parent() */
                    xcb_create_window(c, XCB_COPY_FROM_PARENT, dummy, scr->root, -1, -1, 1, 1, 0,
                                      XCB_WINDOW_CLASS_INPUT_OUTPUT, scr->root_visual,
                                      getenv("MAPPED") ? XCB_CW_OVERRIDE_REDIRECT : 0, &ovr);
                    if (getenv("SHAPE"))   /* SHAPE=1: empty bounding and input shapes, as Wine's dummy parent */
                    {
                        xcb_shape_rectangles(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_BOUNDING, 0, dummy, 0, 0, 0, NULL);
                        xcb_shape_rectangles(c, XCB_SHAPE_SO_SET, XCB_SHAPE_SK_INPUT, 0, dummy, 0, 0, 0, NULL);
                    }
                    if (getenv("MAPPED")) xcb_map_window(c, dummy);
                }
                xcb_reparent_window(c, w, dummy, 0, 0);
            }
            else if (atoi(getenv("ORPHAN")) == 1) xcb_destroy_window(c, w);   /* ORPHAN=3: control, the window stays */
            xcb_flush(c);
            VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
            r = vkQueueSubmit(q, 1, &si, VK_NULL_HANDLE);
            if (!r) r = vkQueueWaitIdle(q);
            if (r) { printf("submit after orphan %d: %d\n", i, r); return 4; }
        }
        if ((i + 1) % 50 == 0) { printf("%d live swapchains\n", i + 1); fflush(stdout); }
    }
    printf("all %d created\n", n);
    return 0;
}

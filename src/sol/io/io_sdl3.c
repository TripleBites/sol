#include "io.h"
#include "io_vulkan.h"
#include "../ui/scene_tree.h"
#include "../ui/control.h"
#include "../ui/color_rect.h"
#include "../ui/vbox_container.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <stdio.h>
#include <stdlib.h>

/* --- SDL3 platform state --- */
static SDL_Window*   g_window;
static VkInstance    g_instance;
static VkSurfaceKHR  g_surface;
static SolVulkan     g_vulkan;

static SceneTree*    g_ui_tree;
static bool          g_should_close;

/* ------------------------------------------------------------------ */
/* Init                                                                */
/* ------------------------------------------------------------------ */
static bool sdl3_init(const char* title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init error: %s", SDL_GetError());
        return false;
    }

    g_window = SDL_CreateWindow(title, width, height,
                                SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!g_window) {
        SDL_Log("SDL_CreateWindow error: %s", SDL_GetError());
        return false;
    }

    /* --- Vulkan instance --- */
    unsigned int ext_count = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&ext_count);

    VkApplicationInfo ai = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName   = title,
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName        = "Sol Engine",
        .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion         = VK_API_VERSION_1_0,
    };
    VkInstanceCreateInfo ici = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo        = &ai,
        .enabledExtensionCount   = ext_count,
        .ppEnabledExtensionNames = exts,
    };
    if (vkCreateInstance(&ici, NULL, &g_instance) != VK_SUCCESS) {
        SDL_Log("vkCreateInstance failed");
        return false;
    }

    if (!SDL_Vulkan_CreateSurface(g_window, g_instance, NULL, &g_surface)) {
        SDL_Log("SDL_Vulkan_CreateSurface error: %s", SDL_GetError());
        return false;
    }

    /* --- Physical device (first available) --- */
    uint32_t dev_count = 0;
    vkEnumeratePhysicalDevices(g_instance, &dev_count, NULL);
    if (dev_count == 0) { SDL_Log("No Vulkan GPUs"); return false; }

    VkPhysicalDevice* devices = malloc(sizeof(*devices) * dev_count);
    vkEnumeratePhysicalDevices(g_instance, &dev_count, devices);
    VkPhysicalDevice phys = devices[0];
    free(devices);

    /* --- Queue family --- */
    uint32_t qf_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qf_count, NULL);
    VkQueueFamilyProperties* qf = malloc(sizeof(*qf) * qf_count);
    vkGetPhysicalDeviceQueueFamilyProperties(phys, &qf_count, qf);

    uint32_t qf_index = UINT32_MAX;
    for (uint32_t i = 0; i < qf_count; i++) {
        VkBool32 ok = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(phys, i, g_surface, &ok);
        if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && ok) { qf_index = i; break; }
    }
    free(qf);

    if (qf_index == UINT32_MAX) { SDL_Log("No suitable queue family"); return false; }

    /* --- Logical device --- */
    float prio = 1.f;
    VkDeviceQueueCreateInfo qci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = qf_index, .queueCount = 1, .pQueuePriorities = &prio,
    };
    const char* dev_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo dci = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount    = 1, .pQueueCreateInfos = &qci,
        .enabledExtensionCount   = 1, .ppEnabledExtensionNames = dev_exts,
    };

    VkDevice dev;
    if (vkCreateDevice(phys, &dci, NULL, &dev) != VK_SUCCESS) {
        SDL_Log("vkCreateDevice failed"); return false;
    }

    VkQueue queue;
    vkGetDeviceQueue(dev, qf_index, 0, &queue);

    /* --- Hand off to the Vulkan renderer --- */
    if (!sol_vulkan_init(&g_vulkan, g_instance, phys, dev, g_surface,
                         qf_index, queue, width, height)) {
        SDL_Log("sol_vulkan_init failed");
        return false;
    }

    /* --- Set up UI pipeline and scene --- */
    if (!sol_vulkan_ui_setup(&g_vulkan)) {
        SDL_Log("sol_vulkan_ui_setup failed");
        return false;
    }

    /* Build a demo UI scene */
    g_ui_tree = scene_tree_create();
    {
        Control *root = control_new(&control_class);
        node_set_name(&root->base, "root");
        root->rect = rect_make(0, 0, (float)width, (float)height);
        root->global_rect = root->rect;

        /* Background */
        ColorRect *bg = color_rect_new();
        node_set_name(&bg->base.base, "bg");
        color_rect_set_color(bg, color_rgba(0.05f, 0.05f, 0.15f, 1.0f));
        node_add_child(&root->base, &bg->base.base);

        /* Centered panel */
        VBoxContainer *panel = vbox_container_new();
        node_set_name(&panel->base.base, "panel");
        control_set_anchor(&panel->base, 0.5f, 0.5f, 0.5f, 0.5f);
        control_set_offset(&panel->base, -150, -200, 150, 200);
        vbox_container_set_separation(panel, 8);

        ColorRect *hdr = color_rect_new();
        node_set_name(&hdr->base.base, "header");
        color_rect_set_color(hdr, color_rgba(0.8f, 0.2f, 0.2f, 1.0f));
        control_set_min_size(&hdr->base, 0, 80);
        control_set_size_flags(&hdr->base, SIZE_FILL, 0);
        node_add_child(&panel->base.base, &hdr->base.base);

        ColorRect *body = color_rect_new();
        node_set_name(&body->base.base, "body");
        color_rect_set_color(body, color_rgba(0.2f, 0.7f, 0.2f, 1.0f));
        control_set_size_flags(&body->base, SIZE_FILL, SIZE_EXPAND | SIZE_FILL);
        node_add_child(&panel->base.base, &body->base.base);

        ColorRect *ftr = color_rect_new();
        node_set_name(&ftr->base.base, "footer");
        color_rect_set_color(ftr, color_rgba(0.2f, 0.3f, 0.9f, 1.0f));
        control_set_min_size(&ftr->base, 0, 60);
        control_set_size_flags(&ftr->base, SIZE_FILL, 0);
        node_add_child(&panel->base.base, &ftr->base.base);

        node_add_child(&root->base, &panel->base.base);
        scene_tree_set_root(g_ui_tree, &root->base);
        node_unref(&root->base);  /* tree now owns the root */
    }

    g_should_close = false;
    SDL_Log("Sol Engine initialized: %s %dx%d", title, width, height);
    return true;
}

/* ------------------------------------------------------------------ */
/* Shutdown                                                            */
/* ------------------------------------------------------------------ */
static void sdl3_shutdown(void) {
    if (g_ui_tree) {
        scene_tree_destroy(g_ui_tree);
        g_ui_tree = NULL;
    }
    sol_vulkan_shutdown(&g_vulkan);

    /* Vulkan device / surface / instance are owned by us, not the renderer */
    if (g_vulkan.device)  vkDestroyDevice(g_vulkan.device, NULL);
    if (g_surface)       vkDestroySurfaceKHR(g_instance, g_surface, NULL);
    if (g_instance)      vkDestroyInstance(g_instance, NULL);
    if (g_window)        SDL_DestroyWindow(g_window);

    SDL_Quit();
    SDL_Log("Sol Engine shut down");
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */
static void handle_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            g_should_close = true;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            sol_vulkan_signal_resize(&g_vulkan);
            break;
        default:
            break;
        }
    }
}

static bool sdl3_update(void) {
    handle_events();
    if (g_should_close) return false;

    /* Process UI */
    if (g_ui_tree) {
        int w, h;
        sol_vulkan_get_size(&g_vulkan, &w, &h);
        /* Update root rect in case of resize */
        if (g_ui_tree->root) {
            Control *root = (Control*)g_ui_tree->root;
            root->rect.w = (float)w;
            root->rect.h = (float)h;
            root->global_rect = root->rect;
        }
        scene_tree_process(g_ui_tree, 0.016f);
        g_vulkan.ui_draw_list = scene_tree_get_draw_list(g_ui_tree);
    }

    if (!sol_vulkan_frame(&g_vulkan)) {
        SDL_Log("sol_vulkan_frame failed");
        return false;
    }
    return true;
}

static void sdl3_get_size(int* w, int* h) {
    sol_vulkan_get_size(&g_vulkan, w, h);
}

/* ------------------------------------------------------------------ */
/* Platform vtable                                                     */
/* ------------------------------------------------------------------ */
static SolPlatform sdl3_platform = {
    .init     = sdl3_init,
    .shutdown = sdl3_shutdown,
    .update   = sdl3_update,
    .get_size = sdl3_get_size,
};

const SolPlatform* sol_io_sdl3(void) {
    return &sdl3_platform;
}

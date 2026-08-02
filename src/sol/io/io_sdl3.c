#include "io.h"
#include "../photon/photon_vulkan.h"
#include "../scene/scene_tree.h"
#include "../scene/control.h"
#include "../scene/color_rect.h"
#include "../scene/vbox_container.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_audio.h>
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

/* --- Audio state --- */
static SDL_AudioDeviceID  g_audio_device = 0;
static SolAudioCallback    g_audio_callback;
static void*               g_audio_userdata;
static SDL_AudioStream*    g_audio_stream;

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
/* Input — pump SDL events directly into InputState                    */
/* ------------------------------------------------------------------ */

static int sdl_scancode_to_sol(SDL_Scancode sc) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z)
        return SOL_KEY_A + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9)
        return SOL_KEY_1 + (sc - SDL_SCANCODE_1);
    if (sc == SDL_SCANCODE_0) return SOL_KEY_0;
    switch (sc) {
    case SDL_SCANCODE_RETURN:    return SOL_KEY_RETURN;
    case SDL_SCANCODE_ESCAPE:    return SOL_KEY_ESCAPE;
    case SDL_SCANCODE_BACKSPACE: return SOL_KEY_BACKSPACE;
    case SDL_SCANCODE_TAB:       return SOL_KEY_TAB;
    case SDL_SCANCODE_SPACE:     return SOL_KEY_SPACE;
    case SDL_SCANCODE_LEFT:      return SOL_KEY_LEFT;
    case SDL_SCANCODE_RIGHT:     return SOL_KEY_RIGHT;
    case SDL_SCANCODE_UP:        return SOL_KEY_UP;
    case SDL_SCANCODE_DOWN:      return SOL_KEY_DOWN;
    case SDL_SCANCODE_LSHIFT:    return SOL_KEY_LSHIFT;
    case SDL_SCANCODE_RSHIFT:    return SOL_KEY_RSHIFT;
    case SDL_SCANCODE_LCTRL:     return SOL_KEY_LCTRL;
    case SDL_SCANCODE_RCTRL:     return SOL_KEY_RCTRL;
    case SDL_SCANCODE_LALT:      return SOL_KEY_LALT;
    case SDL_SCANCODE_RALT:      return SOL_KEY_RALT;
    default: return SOL_KEY_UNKNOWN;
    }
}

static void sdl3_poll_input(SolIO* self) {
    InputState* s = self->input_state;
    if (!s) return;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_EVENT_QUIT:
            s->should_quit = true;
            g_should_close = true;
            break;

        case SDL_EVENT_KEY_DOWN:
            input_state_key_down(s, sdl_scancode_to_sol(e.key.scancode));
            break;

        case SDL_EVENT_KEY_UP:
            input_state_key_up(s, sdl_scancode_to_sol(e.key.scancode));
            break;

        case SDL_EVENT_MOUSE_MOTION:
            input_state_mouse_move(s, e.motion.x, e.motion.y);
            break;

        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            input_state_mouse_button(s, e.button.button, true);
            break;

        case SDL_EVENT_MOUSE_BUTTON_UP:
            input_state_mouse_button(s, e.button.button, false);
            break;

        case SDL_EVENT_MOUSE_WHEEL:
            input_state_mouse_scroll(s, e.wheel.x, e.wheel.y);
            break;

        case SDL_EVENT_FINGER_DOWN:
            input_state_touch(s, (int)e.tfinger.fingerID, true,
                              e.tfinger.x, e.tfinger.y);
            break;

        case SDL_EVENT_FINGER_UP:
            input_state_touch(s, (int)e.tfinger.fingerID, false,
                              e.tfinger.x, e.tfinger.y);
            break;

        case SDL_EVENT_FINGER_MOTION:
            input_state_touch(s, (int)e.tfinger.fingerID, true,
                              e.tfinger.x, e.tfinger.y);
            break;

        case SDL_EVENT_WINDOW_RESIZED:
            sol_vulkan_signal_resize(&g_vulkan);
            s->window_width  = e.window.data1;
            s->window_height = e.window.data2;
            break;

        case SDL_EVENT_GAMEPAD_ADDED: {
            SolEvent se = { .type = SOL_EV_DEVICE_ADDED };
            se.device.device_type  = 6;  /* gamepad */
            se.device.device_index = (int)e.gdevice.which;
            input_state_push_event(s, &se);
            break;
        }
        case SDL_EVENT_GAMEPAD_REMOVED: {
            SolEvent se = { .type = SOL_EV_DEVICE_REMOVED };
            se.device.device_type  = 6;
            se.device.device_index = (int)e.gdevice.which;
            input_state_push_event(s, &se);
            break;
        }
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Update                                                              */
/* ------------------------------------------------------------------ */
static bool sdl3_update(void) {
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
/* Audio                                                               */
/* ------------------------------------------------------------------ */

/* SDL3 audio callback — bridges to SolAudioCallback.
   Called from SDL's audio thread. Must be real-time safe. */
static void SDLCALL sdl_audio_callback(void* userdata,
                                        SDL_AudioStream* stream,
                                        int additional_amount,
                                        int total_amount) {
    (void)userdata;
    (void)total_amount;

    if (!g_audio_callback || additional_amount <= 0) return;

    /* additional_amount is bytes needed; convert to float frames */
    int bytes_per_sample = (int)sizeof(float);
    int channels = 1;  /* we request mono; get from stream spec later */
    int n_frames = additional_amount / (bytes_per_sample * channels);
    if (n_frames <= 0) return;

    /* Temp buffer on the audio thread — small, stack-allocated.
       For larger buffer sizes, this could be preallocated. */
    float temp[2048];
    if (n_frames > 2048) n_frames = 2048;

    g_audio_callback(temp, n_frames, channels, g_audio_userdata);

    SDL_PutAudioStreamData(stream, temp, additional_amount);
}

static bool sdl3_audio_init(int sample_rate, int channels,
                            SolAudioCallback callback, void* userdata) {
    g_audio_callback  = callback;
    g_audio_userdata  = userdata;

    SDL_AudioSpec spec = {
        .format    = SDL_AUDIO_F32,
        .channels  = channels,
        .freq      = sample_rate,
    };

    g_audio_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (g_audio_device == 0) {
        SDL_Log("[sdl3] SDL_OpenAudioDevice failed: %s", SDL_GetError());
        return false;
    }

    /* Create a stream for resampling/conversion, driven by callback */
    g_audio_stream = SDL_CreateAudioStream(&spec, &spec);
    if (!g_audio_stream) {
        SDL_Log("[sdl3] SDL_CreateAudioStream failed: %s", SDL_GetError());
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        return false;
    }

    /* Bind stream to device: device pulls from stream, stream fires callback */
    if (!SDL_BindAudioStream(g_audio_device, g_audio_stream)) {
        SDL_Log("[sdl3] SDL_BindAudioStream failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(g_audio_stream);
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
        return false;
    }

    /* Set the callback that fills the stream */
    SDL_SetAudioStreamGetCallback(g_audio_stream, sdl_audio_callback, NULL);

    /* Start playback */
    if (!SDL_ResumeAudioDevice(g_audio_device)) {
        SDL_Log("[sdl3] SDL_ResumeAudioDevice failed: %s", SDL_GetError());
    }

    SDL_Log("[sdl3] Audio: %dHz, %d ch, device opened", sample_rate, channels);
    return true;
}

static void sdl3_audio_shutdown(void) {
    if (g_audio_device != 0) {
        SDL_PauseAudioDevice(g_audio_device);
        if (g_audio_stream) {
            SDL_UnbindAudioStream(g_audio_stream);
            SDL_DestroyAudioStream(g_audio_stream);
            g_audio_stream = NULL;
        }
        SDL_CloseAudioDevice(g_audio_device);
        g_audio_device = 0;
    }
    g_audio_callback = NULL;
}

static void sdl3_audio_lock(void) {
    /* SDL3 doesn't expose a direct device-level lock.
       We use SDL_PauseAudioDevice as a coarse-grained approach. */
    if (g_audio_device != 0) {
        SDL_PauseAudioDevice(g_audio_device);
    }
}

static void sdl3_audio_unlock(void) {
    if (g_audio_device != 0) {
        SDL_ResumeAudioDevice(g_audio_device);
    }
}

/* ------------------------------------------------------------------ */
/* Platform vtable + constructor                                       */
/* ------------------------------------------------------------------ */
static SolIO sdl3_platform;

static SolIO* sdl3_get_platform(void) {
    sdl3_platform.init           = sdl3_init;
    sdl3_platform.shutdown       = sdl3_shutdown;
    sdl3_platform.update         = sdl3_update;
    sdl3_platform.get_size       = sdl3_get_size;
    sdl3_platform.poll_input     = sdl3_poll_input;
    sdl3_platform.audio_init     = sdl3_audio_init;
    sdl3_platform.audio_shutdown  = sdl3_audio_shutdown;
    sdl3_platform.audio_lock     = sdl3_audio_lock;
    sdl3_platform.audio_unlock   = sdl3_audio_unlock;
    return &sdl3_platform;
}

const SolIO* sol_io_sdl3(void) {
    return sdl3_get_platform();
}

#ifndef SOL_IO_VULKAN_H
#define SOL_IO_VULKAN_H

#include <vulkan/vulkan.h>
#include <stdbool.h>
#include <stdint.h>

struct DrawList;

/*
 * SolVulkan - self-contained Vulkan swapchain renderer.
 *
 * The owning platform (e.g. SDL3) is responsible for:
 *   - Creating the VkInstance, VkSurfaceKHR, VkDevice
 *   - Picking the physical device and graphics queue
 *   - Window events (resize notifications go through sol_vulkan_signal_resize)
 *
 * SolVulkan handles:
 *   - Swapchain + image views
 *   - Render pass
 *   - Graphics pipeline (hardcoded hello-triangle shaders for now)
 *   - Framebuffers
 *   - Sync objects (semaphores, fences)
 *   - Per-frame acquire → draw → submit → present
 */

typedef struct SolVulkan {
    /* Supplied by platform (read-only after init) */
    VkInstance       instance;
    VkPhysicalDevice physical_device;
    VkDevice         device;
    VkSurfaceKHR     surface;
    uint32_t          queue_family;
    VkQueue           queue;

    /* Swapchain */
    VkSwapchainKHR    swapchain;
    VkFormat          format;
    VkExtent2D        extent;
    uint32_t          image_count;
    VkImage           images[8];
    VkImageView       views[8];
    VkFramebuffer     framebuffers[8];

    /* Pipeline */
    VkRenderPass      render_pass;

    /* Command buffers */
    VkCommandPool     command_pool;
    VkCommandBuffer   command_buffers[2];  /* one per frame in flight */

    /* Sync */
    VkSemaphore       image_available[2];
    VkSemaphore       render_finished[2];
    VkFence           in_flight[2];
    uint32_t          frame_index;

    /* State */
    bool              should_recreate;
    int               width, height;

    /* UI rendering */
    VkPipeline        ui_pipeline;
    VkPipelineLayout  ui_pipeline_layout;
    VkBuffer          ui_vertex_buffer;
    VkDeviceMemory    ui_vertex_memory;
    uint32_t          ui_vertex_count;
    const void*       ui_draw_list;  /* set by platform before frame */

    /* 2D batch renderer */
    void*             render2d;
    uint32_t          render2d_vertex_count;
} SolVulkan;

/* Initialize all Vulkan rendering state. Returns false on failure. */
bool sol_vulkan_init(SolVulkan* vk,
                     VkInstance instance, VkPhysicalDevice physical_device,
                     VkDevice device, VkSurfaceKHR surface,
                     uint32_t queue_family, VkQueue queue,
                     int width, int height);

/* Destroy all Vulkan objects owned by SolVulkan. Does NOT destroy
   instance, device, or surface (those are owned by the platform). */
void sol_vulkan_shutdown(SolVulkan* vk);

/*
 * Render one frame:
 *   wait fence → acquire → record → submit → present
 * Returns true on success.
 * Returns false on fatal error (e.g. device lost).
 * If the swapchain is out of date the function recreates it internally
 * and returns true (caller can continue the loop).
 */
bool sol_vulkan_frame(SolVulkan* vk);

/* Notify that the surface has been resized. The swapchain will be
   recreated on the next call to sol_vulkan_frame. */
void sol_vulkan_signal_resize(SolVulkan* vk);

/* Query current logical size (pixels). */
void sol_vulkan_get_size(const SolVulkan* vk, int* w, int* h);

/* UI rendering: set up the UI pipeline (call once after sol_vulkan_init). */
bool sol_vulkan_ui_setup(SolVulkan* vk);

/* UI rendering: record draw commands for a DrawList into a command buffer.
   Must be called between vkBeginCommandBuffer/vkEndCommandBuffer,
   inside a render pass. */
void sol_vulkan_ui_draw(SolVulkan* vk, VkCommandBuffer cmd, const struct DrawList* dl);

/* 2D batch renderer setup and drawing */
bool sol_vulkan_2d_setup(SolVulkan* vk);
void sol_vulkan_2d_flush(SolVulkan* vk, VkCommandBuffer cmd);

#endif /* SOL_IO_VULKAN_H */

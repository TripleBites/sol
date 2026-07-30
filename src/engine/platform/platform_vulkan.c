#include "platform_vulkan.h"
#include "../shaders.h"
#include "../ui/draw_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static VkResult create_shader_module(VkDevice device, const unsigned char* code,
                                     unsigned int size, VkShaderModule* out) {
    VkShaderModuleCreateInfo ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = (const uint32_t*)code,
    };
    return vkCreateShaderModule(device, &ci, NULL, out);
}

/* ------------------------------------------------------------------ */
/* Swapchain                                                           */
/* ------------------------------------------------------------------ */
static void destroy_swapchain_objects(SolVulkan* vk) {
    for (uint32_t i = 0; i < vk->image_count; i++) {
        if (vk->framebuffers[i]) {
            vkDestroyFramebuffer(vk->device, vk->framebuffers[i], NULL);
            vk->framebuffers[i] = VK_NULL_HANDLE;
        }
        if (vk->views[i]) {
            vkDestroyImageView(vk->device, vk->views[i], NULL);
            vk->views[i] = VK_NULL_HANDLE;
        }
    }
    if (vk->swapchain) {
        vkDestroySwapchainKHR(vk->device, vk->swapchain, NULL);
        vk->swapchain = VK_NULL_HANDLE;
    }
}

static bool build_swapchain(SolVulkan* vk) {
    destroy_swapchain_objects(vk);

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->physical_device, vk->surface, &caps);

    /* Extent */
    if (caps.currentExtent.width != UINT32_MAX) {
        vk->extent = caps.currentExtent;
    } else {
        /* Wayland & friends – use the size we were given */
        vk->extent.width  = (uint32_t)(vk->width  > 0 ? vk->width  : 800);
        vk->extent.height = (uint32_t)(vk->height > 0 ? vk->height : 600);
        if (vk->extent.width  < caps.minImageExtent.width)  vk->extent.width  = caps.minImageExtent.width;
        if (vk->extent.width  > caps.maxImageExtent.width)  vk->extent.width  = caps.maxImageExtent.width;
        if (vk->extent.height < caps.minImageExtent.height) vk->extent.height = caps.minImageExtent.height;
        if (vk->extent.height > caps.maxImageExtent.height) vk->extent.height = caps.maxImageExtent.height;
    }

    /* Image count */
    uint32_t count = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && count > caps.maxImageCount) count = caps.maxImageCount;
    if (count > 8) count = 8;

    /* Format */
    vk->format = VK_FORMAT_B8G8R8A8_SRGB;
    {
        uint32_t n;
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &n, NULL);
        VkSurfaceFormatKHR* fmts = malloc(sizeof(*fmts) * n);
        vkGetPhysicalDeviceSurfaceFormatsKHR(vk->physical_device, vk->surface, &n, fmts);
        if (n == 1 && fmts[0].format == VK_FORMAT_UNDEFINED) {
            vk->format = VK_FORMAT_B8G8R8A8_SRGB;
        } else {
            vk->format = fmts[0].format;
        }
        free(fmts);
    }

    VkSwapchainCreateInfoKHR sci = {
        .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface          = vk->surface,
        .minImageCount    = count,
        .imageFormat      = vk->format,
        .imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent      = vk->extent,
        .imageArrayLayers = 1,
        .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .preTransform     = caps.currentTransform,
        .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode      = VK_PRESENT_MODE_FIFO_KHR,
        .clipped          = VK_TRUE,
    };

    if (vkCreateSwapchainKHR(vk->device, &sci, NULL, &vk->swapchain) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vkCreateSwapchainKHR failed\n");
        return false;
    }

    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &count, NULL);
    if (count > 8) count = 8;
    vk->image_count = count;
    vkGetSwapchainImagesKHR(vk->device, vk->swapchain, &vk->image_count, vk->images);

    /* Image views */
    for (uint32_t i = 0; i < vk->image_count; i++) {
        VkImageViewCreateInfo ivci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk->format,
            .components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                            VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY },
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
        };
        if (vkCreateImageView(vk->device, &ivci, NULL, &vk->views[i]) != VK_SUCCESS) {
            fprintf(stderr, "[vulkan] vkCreateImageView[%u] failed\n", i);
            return false;
        }
    }

    vk->width  = (int)vk->extent.width;
    vk->height = (int)vk->extent.height;
    return true;
}

/* ------------------------------------------------------------------ */
/* Render pass, pipeline, framebuffers                                 */
/* ------------------------------------------------------------------ */
static bool build_render_pass(SolVulkan* vk) {
    VkAttachmentDescription color = {
        .format         = vk->format,
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };
    VkAttachmentReference ref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription subpass = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &ref,
    };
    VkSubpassDependency dep = {
        .srcSubpass    = VK_SUBPASS_EXTERNAL,
        .dstSubpass    = 0,
        .srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };
    VkRenderPassCreateInfo rpci = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments    = &color,
        .subpassCount    = 1,
        .pSubpasses      = &subpass,
        .dependencyCount = 1,
        .pDependencies   = &dep,
    };
    return vkCreateRenderPass(vk->device, &rpci, NULL, &vk->render_pass) == VK_SUCCESS;
}

static bool build_framebuffers(SolVulkan* vk) {
    for (uint32_t i = 0; i < vk->image_count; i++) {
        VkImageView att[] = { vk->views[i] };
        VkFramebufferCreateInfo fci = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = vk->render_pass,
            .attachmentCount = 1,
            .pAttachments    = att,
            .width           = vk->extent.width,
            .height          = vk->extent.height,
            .layers          = 1,
        };
        if (vkCreateFramebuffer(vk->device, &fci, NULL, &vk->framebuffers[i]) != VK_SUCCESS) {
            fprintf(stderr, "[vulkan] vkCreateFramebuffer[%u] failed\n", i);
            return false;
        }
    }
    return true;
}

static bool build_pipeline(SolVulkan* vk) {
    VkShaderModule vert, frag;
    if (create_shader_module(vk->device, vertex_spv, vertex_spv_len, &vert) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vertex shader module failed\n");
        return false;
    }
    if (create_shader_module(vk->device, fragment_spv, fragment_spv_len, &frag) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] fragment shader module failed\n");
        vkDestroyShaderModule(vk->device, vert, NULL);
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main" },
    };

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dyn,
    };

    VkPipelineVertexInputStateCreateInfo vert_input = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };
    VkPipelineInputAssemblyStateCreateInfo input_asm = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkViewport vp = { 0, 0, (float)vk->extent.width, (float)vk->extent.height, 0.f, 1.f };
    VkRect2D   sc = { {0, 0}, { vk->extent.width, vk->extent.height } };
    VkPipelineViewportStateCreateInfo vp_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &vp,
        .scissorCount  = 1, .pScissors  = &sc,
    };

    VkPipelineRasterizationStateCreateInfo raster = {
        .sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode    = VK_CULL_MODE_BACK_BIT,
        .frontFace   = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth   = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState blend_att = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo blend = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &blend_att,
    };

    VkPipelineLayoutCreateInfo plci = { .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    if (vkCreatePipelineLayout(vk->device, &plci, NULL, &vk->pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vkCreatePipelineLayout failed\n");
        vkDestroyShaderModule(vk->device, vert, NULL);
        vkDestroyShaderModule(vk->device, frag, NULL);
        return false;
    }

    VkGraphicsPipelineCreateInfo gpci = {
        .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount          = 2, .pStages = stages,
        .pVertexInputState   = &vert_input,
        .pInputAssemblyState = &input_asm,
        .pViewportState      = &vp_state,
        .pRasterizationState = &raster,
        .pMultisampleState   = &ms,
        .pColorBlendState    = &blend,
        .pDynamicState       = &dyn_state,
        .layout              = vk->pipeline_layout,
        .renderPass          = vk->render_pass,
        .subpass             = 0,
    };

    VkResult r = vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &gpci, NULL, &vk->pipeline);

    vkDestroyShaderModule(vk->device, vert, NULL);
    vkDestroyShaderModule(vk->device, frag, NULL);

    if (r != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vkCreateGraphicsPipelines failed\n");
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Sync & command buffers                                              */
/* ------------------------------------------------------------------ */
static bool build_sync(SolVulkan* vk) {
    VkSemaphoreCreateInfo sci = { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo     fci = { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                   .flags = VK_FENCE_CREATE_SIGNALED_BIT };
    for (int i = 0; i < 2; i++) {
        if (vkCreateSemaphore(vk->device, &sci, NULL, &vk->image_available[i]) != VK_SUCCESS) return false;
        if (vkCreateSemaphore(vk->device, &sci, NULL, &vk->render_finished[i]) != VK_SUCCESS) return false;
        if (vkCreateFence(vk->device, &fci, NULL, &vk->in_flight[i]) != VK_SUCCESS) return false;
    }
    return true;
}

static bool build_command_buffers(SolVulkan* vk) {
    VkCommandPoolCreateInfo cpci = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk->queue_family,
    };
    if (vkCreateCommandPool(vk->device, &cpci, NULL, &vk->command_pool) != VK_SUCCESS) return false;

    VkCommandBufferAllocateInfo cbai = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 2,
    };
    return vkAllocateCommandBuffers(vk->device, &cbai, vk->command_buffers) == VK_SUCCESS;
}

/* ------------------------------------------------------------------ */
/* Per-frame recording                                                 */
/* ------------------------------------------------------------------ */
static void record_commands(SolVulkan* vk, VkCommandBuffer cmd, uint32_t image_index) {
    VkCommandBufferBeginInfo bi = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    vkBeginCommandBuffer(cmd, &bi);

    VkClearValue clear = { .color = {{ 0.f, 0.f, 0.f, 1.f }} };
    VkRenderPassBeginInfo rpbi = {
        .sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass  = vk->render_pass,
        .framebuffer = vk->framebuffers[image_index],
        .renderArea  = {{ 0, 0 }, { vk->extent.width, vk->extent.height }},
        .clearValueCount = 1,
        .pClearValues    = &clear,
    };
    vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipeline);

    VkViewport vp = { 0, 0, (float)vk->extent.width, (float)vk->extent.height, 0.f, 1.f };
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc = {{ 0, 0 }, { vk->extent.width, vk->extent.height }};
    vkCmdSetScissor(cmd, 0, 1, &sc);

    vkCmdDraw(cmd, 3, 1, 0, 0);

    /* Render UI on top */
    if (vk->ui_pipeline != VK_NULL_HANDLE && vk->ui_draw_list) {
        sol_vulkan_ui_draw(vk, cmd, (const DrawList*)vk->ui_draw_list);
    }

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

/* ------------------------------------------------------------------ */
/* UI Rendering                                                        */
/* ------------------------------------------------------------------ */

#define MAX_UI_VERTICES (1024 * 6)  /* 1024 rects × 6 vertices each */

typedef struct {
    float pos[2];
    float color[4];
} UIVertex;

static uint32_t find_memory_type(SolVulkan* vk, uint32_t type_filter,
                                  VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem;
    vkGetPhysicalDeviceMemoryProperties(vk->physical_device, &mem);
    for (uint32_t i = 0; i < mem.memoryTypeCount; i++) {
        if ((type_filter & (1u << i)) &&
            (mem.memoryTypes[i].propertyFlags & props) == props) return i;
    }
    return 0;
}

static bool create_buffer(SolVulkan* vk, VkDeviceSize size,
                          VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                          VkBuffer* buf, VkDeviceMemory* mem) {
    VkBufferCreateInfo bci = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size, .usage = usage, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    if (vkCreateBuffer(vk->device, &bci, NULL, buf) != VK_SUCCESS) return false;

    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(vk->device, *buf, &req);

    VkMemoryAllocateInfo ai = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = find_memory_type(vk, req.memoryTypeBits, props),
    };
    if (vkAllocateMemory(vk->device, &ai, NULL, mem) != VK_SUCCESS) return false;
    vkBindBufferMemory(vk->device, *buf, *mem, 0);
    return true;
}

bool sol_vulkan_ui_setup(SolVulkan* vk) {
    /* --- Shader modules --- */
    VkShaderModule vert, frag;
    VkShaderModuleCreateInfo smci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
    };
    smci.codeSize = ui_vert_spv_len; smci.pCode = (const uint32_t*)ui_vert_spv;
    if (vkCreateShaderModule(vk->device, &smci, NULL, &vert) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] UI vert shader failed\n");
        return false;
    }
    smci.codeSize = ui_frag_spv_len; smci.pCode = (const uint32_t*)ui_frag_spv;
    if (vkCreateShaderModule(vk->device, &smci, NULL, &frag) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] UI frag shader failed\n");
        vkDestroyShaderModule(vk->device, vert, NULL);
        return false;
    }

    /* --- Pipeline layout (push constant for screen size) --- */
    VkPushConstantRange pcr = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0, .size = 8,  /* vec2 screenSize = 8 bytes */
    };
    VkPipelineLayoutCreateInfo plci = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pcr,
    };
    if (vkCreatePipelineLayout(vk->device, &plci, NULL,
                               &vk->ui_pipeline_layout) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] UI pipeline layout failed\n");
        vkDestroyShaderModule(vk->device, vert, NULL);
        vkDestroyShaderModule(vk->device, frag, NULL);
        return false;
    }

    /* --- Vertex input --- */
    VkVertexInputBindingDescription bindings[] = {{
        .binding = 0, .stride = sizeof(UIVertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    }};
    VkVertexInputAttributeDescription attrs[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32_SFLOAT, .offset = 0 },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32A32_SFLOAT,
          .offset = 8 },
    };
    VkPipelineVertexInputStateCreateInfo vis = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1, .pVertexBindingDescriptions = bindings,
        .vertexAttributeDescriptionCount = 2, .pVertexAttributeDescriptions = attrs,
    };

    /* --- Shader stages --- */
    VkPipelineShaderStageCreateInfo stages[] = {
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT, .module = vert, .pName = "main" },
        { .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT, .module = frag, .pName = "main" },
    };

    VkPipelineInputAssemblyStateCreateInfo ias = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,  /* UI: no culling */
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.f,
    };

    VkPipelineMultisampleStateCreateInfo ms = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    /* Enable alpha blending */
    VkPipelineColorBlendAttachmentState cba = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    VkPipelineColorBlendStateCreateInfo cbs = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1, .pAttachments = &cba,
    };

    VkDynamicState dyn[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo ds = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2, .pDynamicStates = dyn,
    };

    VkViewport vp = {0, 0, (float)vk->extent.width, (float)vk->extent.height, 0, 1};
    VkRect2D sc = {{0, 0}, {vk->extent.width, vk->extent.height}};
    VkPipelineViewportStateCreateInfo vps = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &vp,
        .scissorCount = 1, .pScissors = &sc,
    };

    VkGraphicsPipelineCreateInfo gpci = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2, .pStages = stages,
        .pVertexInputState = &vis,
        .pInputAssemblyState = &ias,
        .pViewportState = &vps,
        .pRasterizationState = &rs,
        .pMultisampleState = &ms,
        .pColorBlendState = &cbs,
        .pDynamicState = &ds,
        .layout = vk->ui_pipeline_layout,
        .renderPass = vk->render_pass,
        .subpass = 0,
    };

    VkResult r = vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE,
                                            1, &gpci, NULL, &vk->ui_pipeline);
    vkDestroyShaderModule(vk->device, vert, NULL);
    vkDestroyShaderModule(vk->device, frag, NULL);

    if (r != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] UI pipeline failed\n");
        return false;
    }

    /* --- Vertex buffer --- */
    VkDeviceSize buf_size = sizeof(UIVertex) * MAX_UI_VERTICES;
    if (!create_buffer(vk, buf_size,
                       VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                       &vk->ui_vertex_buffer, &vk->ui_vertex_memory)) {
        fprintf(stderr, "[vulkan] UI vertex buffer failed\n");
        return false;
    }

    fprintf(stderr, "[vulkan] UI pipeline initialized\n");
    return true;
}

void sol_vulkan_ui_draw(SolVulkan* vk, VkCommandBuffer cmd,
                        const DrawList* dl) {
    if (!dl || draw_list_cmd_count(dl) == 0) return;

    /* Build vertex data from DrawList */
    UIVertex* verts;
    vkMapMemory(vk->device, vk->ui_vertex_memory, 0,
                sizeof(UIVertex) * MAX_UI_VERTICES, 0, (void**)&verts);

    uint32_t vi = 0;
    const size_t n = draw_list_cmd_count(dl);

    /* Scissor stack */
    VkRect2D scissor_stack[16];
    int scissor_depth = 0;
    VkRect2D current_scissor = {{0, 0}, {vk->extent.width, vk->extent.height}};

    for (size_t i = 0; i < n && vi + 6 <= MAX_UI_VERTICES; i++) {
        DrawCmd cmd = draw_list_get_cmd(dl, i);

        switch (cmd.type) {
        case DRAW_CMD_CLIP_PUSH: {
            /* Save current scissor, intersect with clip rect */
            if (scissor_depth < 16) {
                scissor_stack[scissor_depth++] = current_scissor;
                VkRect2D clip = {
                    {(int32_t)cmd.rect.x, (int32_t)cmd.rect.y},
                    {(uint32_t)cmd.rect.w, (uint32_t)cmd.rect.h},
                };
                /* Intersect with current */
                int32_t l = current_scissor.offset.x > clip.offset.x
                    ? current_scissor.offset.x : clip.offset.x;
                int32_t t = current_scissor.offset.y > clip.offset.y
                    ? current_scissor.offset.y : clip.offset.y;
                int32_t r = (current_scissor.offset.x + (int32_t)current_scissor.extent.width)
                    < (clip.offset.x + (int32_t)clip.extent.width)
                    ? (current_scissor.offset.x + (int32_t)current_scissor.extent.width)
                    : (clip.offset.x + (int32_t)clip.extent.width);
                int32_t b = (current_scissor.offset.y + (int32_t)current_scissor.extent.height)
                    < (clip.offset.y + (int32_t)clip.extent.height)
                    ? (current_scissor.offset.y + (int32_t)current_scissor.extent.height)
                    : (clip.offset.y + (int32_t)clip.extent.height);
                current_scissor.offset.x = l;
                current_scissor.offset.y = t;
                current_scissor.extent.width  = (uint32_t)(r > l ? r - l : 0);
                current_scissor.extent.height = (uint32_t)(b > t ? b - t : 0);
            }
            break;
        }
        case DRAW_CMD_CLIP_POP:
            if (scissor_depth > 0) {
                current_scissor = scissor_stack[--scissor_depth];
            }
            break;
        case DRAW_CMD_RECT_FILLED: {
            float x = cmd.rect.x, y = cmd.rect.y;
            float w = cmd.rect.w, h = cmd.rect.h;
            float cr = cmd.color.r, cg = cmd.color.g;
            float cb = cmd.color.b, ca = cmd.color.a;

            /* Two triangles: (x,y), (x+w,y), (x+w,y+h), (x,y), (x+w,y+h), (x,y+h) */
            UIVertex v[6] = {
                {{x,   y  }, {cr, cg, cb, ca}},
                {{x+w, y  }, {cr, cg, cb, ca}},
                {{x+w, y+h}, {cr, cg, cb, ca}},
                {{x,   y  }, {cr, cg, cb, ca}},
                {{x+w, y+h}, {cr, cg, cb, ca}},
                {{x,   y+h}, {cr, cg, cb, ca}},
            };
            memcpy(&verts[vi], v, sizeof(v));
            vi += 6;
            break;
        }
        default:
            break;
        }
    }

    vkUnmapMemory(vk->device, vk->ui_vertex_memory);

    if (vi == 0) return;

    /* Draw */
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->ui_pipeline);

    float screen_size[2] = {(float)vk->extent.width, (float)vk->extent.height};
    vkCmdPushConstants(cmd, vk->ui_pipeline_layout,
                       VK_SHADER_STAGE_VERTEX_BIT, 0, 8, screen_size);

    VkViewport vp = {0, 0, (float)vk->extent.width, (float)vk->extent.height, 0.f, 1.f};
    vkCmdSetViewport(cmd, 0, 1, &vp);
    vkCmdSetScissor(cmd, 0, 1, &current_scissor);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vk->ui_vertex_buffer, &offset);
    vkCmdDraw(cmd, vi, 1, 0, 0);
}
bool sol_vulkan_init(SolVulkan* vk,
                     VkInstance instance, VkPhysicalDevice physical_device,
                     VkDevice device, VkSurfaceKHR surface,
                     uint32_t queue_family, VkQueue queue,
                     int width, int height) {
    memset(vk, 0, sizeof(*vk));
    vk->instance        = instance;
    vk->physical_device = physical_device;
    vk->device           = device;
    vk->surface          = surface;
    vk->queue_family     = queue_family;
    vk->queue            = queue;
    vk->width            = width;
    vk->height           = height;

    if (!build_swapchain(vk))       return false;
    if (!build_render_pass(vk))     return false;
    if (!build_pipeline(vk))        return false;
    if (!build_framebuffers(vk))    return false;
    if (!build_command_buffers(vk)) return false;
    if (!build_sync(vk))            return false;

    vk->frame_index     = 0;
    vk->should_recreate = false;
    return true;
}

void sol_vulkan_shutdown(SolVulkan* vk) {
    vkDeviceWaitIdle(vk->device);

    for (int i = 0; i < 2; i++) {
        if (vk->image_available[i])  vkDestroySemaphore(vk->device, vk->image_available[i], NULL);
        if (vk->render_finished[i]) vkDestroySemaphore(vk->device, vk->render_finished[i], NULL);
        if (vk->in_flight[i])       vkDestroyFence(vk->device, vk->in_flight[i], NULL);
    }

    destroy_swapchain_objects(vk);

    if (vk->ui_vertex_memory) vkFreeMemory(vk->device, vk->ui_vertex_memory, NULL);
    if (vk->ui_vertex_buffer) vkDestroyBuffer(vk->device, vk->ui_vertex_buffer, NULL);
    if (vk->ui_pipeline)      vkDestroyPipeline(vk->device, vk->ui_pipeline, NULL);
    if (vk->ui_pipeline_layout) vkDestroyPipelineLayout(vk->device, vk->ui_pipeline_layout, NULL);
    if (vk->command_pool)    vkDestroyCommandPool(vk->device, vk->command_pool, NULL);
    if (vk->pipeline)        vkDestroyPipeline(vk->device, vk->pipeline, NULL);
    if (vk->pipeline_layout) vkDestroyPipelineLayout(vk->device, vk->pipeline_layout, NULL);
    if (vk->render_pass)     vkDestroyRenderPass(vk->device, vk->render_pass, NULL);
}

bool sol_vulkan_frame(SolVulkan* vk) {
    uint32_t fi = vk->frame_index;

    vkWaitForFences(vk->device, 1, &vk->in_flight[fi], VK_TRUE, UINT64_MAX);

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(vk->device, vk->swapchain, UINT64_MAX,
                                             vk->image_available[fi],
                                             VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || vk->should_recreate) {
        vk->should_recreate = false;
        if (!build_swapchain(vk) || !build_framebuffers(vk)) {
            fprintf(stderr, "[vulkan] swapchain recreation failed\n");
            return false;
        }
        return true;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        fprintf(stderr, "[vulkan] vkAcquireNextImageKHR: %d\n", result);
        return false;
    }

    vkResetFences(vk->device, 1, &vk->in_flight[fi]);

    vkResetCommandBuffer(vk->command_buffers[fi], 0);
    record_commands(vk, vk->command_buffers[fi], image_index);

    VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkSubmitInfo si = {
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &vk->image_available[fi],
        .pWaitDstStageMask    = wait_stages,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &vk->command_buffers[fi],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &vk->render_finished[fi],
    };

    if (vkQueueSubmit(vk->queue, 1, &si, vk->in_flight[fi]) != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vkQueueSubmit failed\n");
        return false;
    }

    VkPresentInfoKHR pi = {
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &vk->render_finished[fi],
        .swapchainCount      = 1,
        .pSwapchains        = &vk->swapchain,
        .pImageIndices      = &image_index,
    };

    result = vkQueuePresentKHR(vk->queue, &pi);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        vk->should_recreate = true;
    } else if (result != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] vkQueuePresentKHR: %d\n", result);
        return false;
    }

    vk->frame_index = (fi + 1) % 2;
    return true;
}

void sol_vulkan_signal_resize(SolVulkan* vk) {
    vk->should_recreate = true;
}

void sol_vulkan_get_size(const SolVulkan* vk, int* w, int* h) {
    *w = vk->width;
    *h = vk->height;
}

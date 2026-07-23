const std = @import("std");

const zemscripten = @import("zemscripten");
pub const panic = zemscripten.panic;
pub const std_options = std.Options{
    .logFn = zemscripten.log,
};

// ── WebGL / GLES2 thin externs ───────────────────────────────────────────────
const GL_COLOR_BUFFER_BIT: u32 = 0x00004000;
const GL_DEPTH_BUFFER_BIT: u32 = 0x00000100;
const GL_STENCIL_BUFFER_BIT: u32 = 0x00000400;

extern fn glClearColor(r: f32, g: f32, b: f32, a: f32) void;
extern fn glClear(mask: u32) void;
extern fn glViewport(x: i32, y: i32, w: i32, h: i32) void;

// ── Emscripten WebGL context helpers ─────────────────────────────────────────
const EmscriptenWebGLContextAttributes = extern struct {
    alpha: i32 = 1,
    depth: i32 = 1,
    stencil: i32 = 0,
    antialias: i32 = 1,
    premultipliedAlpha: i32 = 1,
    preserveDrawingBuffer: i32 = 0,
    preferLowPowerToHighPerformance: i32 = 0,
    failIfMajorPerformanceCaveat: i32 = 0,
    majorVersion: i32 = 2,
    minorVersion: i32 = 0,
    enableExtensionsByDefault: i32 = 1,
    explicitSwapControl: i32 = 0,
    proxyContextToMainThread: i32 = 0,
    renderViaOffscreenBackBuffer: i32 = 0,
};

extern fn emscripten_webgl_create_context(
    target: [*:0]const u8,
    attrs: [*c]const EmscriptenWebGLContextAttributes,
) i32;
extern fn emscripten_webgl_make_context_current(ctx: i32) i32;
extern fn emscripten_get_canvas_element_size(
    target: [*:0]const u8,
    width: *i32,
    height: *i32,
) i32;

// ── Game state ───────────────────────────────────────────────────────────────
var frame_count: u32 = 0;

fn mainLoop() callconv(.c) void {
    // Resize viewport to match canvas each frame (handles window resize)
    var w: i32 = 0;
    var h: i32 = 0;
    _ = emscripten_get_canvas_element_size("canvas", &w, &h);
    glViewport(0, 0, w, h);

    // Pulsing background color
    const t: f32 = @floatFromInt(frame_count);
    const r = (@sin(t * 0.008) + 1.0) * 0.5;
    const g = (@sin(t * 0.011 + 2.0) + 1.0) * 0.5;
    const b = (@sin(t * 0.014 + 4.0) + 1.0) * 0.5;

    glClearColor(r, g, b, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    frame_count += 1;
}

// ── Entry point ──────────────────────────────────────────────────────────────
export fn main() c_int {
    std.log.info("sol: initializing WebGL…", .{});

    const attrs = EmscriptenWebGLContextAttributes{
        .majorVersion = 2,
    };
    const ctx = emscripten_webgl_create_context("canvas", &attrs);
    if (ctx <= 0) {
        std.log.err("failed to create WebGL 2 context; trying WebGL 1…", .{});

        const fallback = EmscriptenWebGLContextAttributes{
            .majorVersion = 1,
        };
        const fb_ctx = emscripten_webgl_create_context("canvas", &fallback);
        if (fb_ctx <= 0) {
            std.log.err("failed to create WebGL 1 context", .{});
            return 1;
        }
        _ = emscripten_webgl_make_context_current(fb_ctx);
    } else {
        _ = emscripten_webgl_make_context_current(ctx);
    }

    std.log.info("sol: entering main loop", .{});
    zemscripten.setMainLoop(mainLoop, 0, true);

    return 0;
}

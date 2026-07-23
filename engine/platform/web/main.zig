// ──────────────────────────────────────────────────────────────────────────────
//  Sol Engine — Web / WASM Platform Layer
//
//  This is the Handmade Hero-style "platform layer" for the browser.
//  It owns the main loop, polls input from the DOM, calls into the engine
//  for each frame, and blits the resulting pixel buffer onto a <canvas>.
//  The engine code (engine/core/engine.zig) knows nothing about the browser.
// ──────────────────────────────────────────────────────────────────────────────

const std = @import("std");
const engine = @import("engine");
const zemscripten = @import("zemscripten");

pub const panic = zemscripten.panic;
pub const std_options = std.Options{
    .logFn = zemscripten.log,
};

const WIDTH: i32 = @intCast(engine.WIDTH);
const HEIGHT: i32 = @intCast(engine.HEIGHT);

// ── Externs from canvas.js (Emscripten JS library) ───────────────────────────
extern fn canvas_init(width: i32, height: i32) void;
extern fn canvas_put_pixels(ptr: [*]const u8, size: i32) void;
extern fn key_is_down(key_code: [*:0]const u8) i32;
extern fn mouse_get_x() i32;
extern fn mouse_get_y() i32;
extern fn mouse_button_down(btn: i32) i32;

// ── Emscripten timing ────────────────────────────────────────────────────────
extern fn emscripten_get_now() f64;

// ── Global engine context ────────────────────────────────────────────────────
var g_engine: engine.EngineContext = undefined;
var g_last_time_ms: f64 = 0.0;

/// Called every frame via requestAnimationFrame (zemscripten.setMainLoop).
fn mainLoop() callconv(.c) void {
    // ── Timing ────────────────────────────────────────────────────────────
    const now_ms = emscripten_get_now();
    var dt_ms = now_ms - g_last_time_ms;
    if (g_last_time_ms == 0.0) dt_ms = 16.667; // first frame
    g_last_time_ms = now_ms;

    // Clamp dt to avoid spiral-of-death after tab-away
    if (dt_ms > 100.0) dt_ms = 16.667;
    const dt: f32 = @floatCast(dt_ms / 1000.0);

    // ── Gather input ──────────────────────────────────────────────────────
    var input = engine.EngineInput{
        .dt_for_frame = dt,
        .mouse_x = mouse_get_x(),
        .mouse_y = mouse_get_y(),
    };

    // Keyboard (using DOM key codes)
    input.move_up.ended_down   = key_is_down("ArrowUp")    != 0 or key_is_down("KeyW") != 0;
    input.move_down.ended_down = key_is_down("ArrowDown")  != 0 or key_is_down("KeyS") != 0;
    input.move_left.ended_down = key_is_down("ArrowLeft")  != 0 or key_is_down("KeyA") != 0;
    input.move_right.ended_down= key_is_down("ArrowRight") != 0 or key_is_down("KeyD") != 0;
    input.action1.ended_down   = key_is_down("Space")      != 0;

    // Mouse buttons
    inline for (0..3) |i| {
        input.mouse_buttons[i].ended_down = mouse_button_down(@intCast(i)) != 0;
    }

    // ── Compute transitions (Handmade Hero style) ─────────────────────────
    computeTransitions(&input.move_up,    &g_engine.prev_input.move_up);
    computeTransitions(&input.move_down,  &g_engine.prev_input.move_down);
    computeTransitions(&input.move_left,  &g_engine.prev_input.move_left);
    computeTransitions(&input.move_right, &g_engine.prev_input.move_right);
    computeTransitions(&input.action1,    &g_engine.prev_input.action1);
    inline for (0..3) |i| {
        computeTransitions(&input.mouse_buttons[i], &g_engine.prev_input.mouse_buttons[i]);
    }

    // ── Run one frame of the engine ───────────────────────────────────────
    g_engine.updateAndRender(input);

    // ── Blit the framebuffer to the canvas ────────────────────────────────
    canvas_put_pixels(&g_engine.framebuffer, engine.BUFFER_SIZE);
}

/// Compares the current raw button state with the previous frame's state
/// and fills in `half_transition_count`.
fn computeTransitions(btn: *engine.Button, prev: *engine.Button) void {
    if (btn.ended_down != prev.ended_down) {
        btn.half_transition_count = 1;
    } else {
        btn.half_transition_count = 0;
    }
}

// ── Entry point (called from Emscripten) ─────────────────────────────────────
export fn main() c_int {
    // Initialise the canvas
    canvas_init(WIDTH, HEIGHT);

    // Boot the engine
    g_engine = engine.EngineContext{};
    g_engine.init();

    // Kick off the rAF loop
    zemscripten.setMainLoop(mainLoop, 0, true);

    return 0;
}

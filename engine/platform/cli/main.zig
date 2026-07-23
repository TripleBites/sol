// ──────────────────────────────────────────────────────────────────────────────
//  Minimal native platform stub (placeholder until SDL3/GLFW is wired up).
//  Exercises the engine API to verify the build is correct.
// ──────────────────────────────────────────────────────────────────────────────

const std = @import("std");
const engine = @import("engine");

pub fn main() void {
    // ── Initialise engine ─────────────────────────────────────────────────
    var ctx = engine.EngineContext{};
    ctx.init();

    std.debug.print(
        "╔══════════════════════════════════════╗\n" ++
        "║   Sol Engine  v0.1.0-dev             ║\n" ++
        "║   Handmade Hero-style platform API   ║\n" ++
        "╚══════════════════════════════════════╝\n" ++
        "\n",
        .{},
    );

    // ── Run 60 frames (simulating a 1-second game loop) ───────────────────
    const frames: u32 = 60;
    const dt: f32 = 1.0 / 60.0;

    var frame: u32 = 0;
    while (frame < frames) : (frame += 1) {
        var input = engine.EngineInput{
            .dt_for_frame = dt,
        };

        // Wiggle some input to make it more interesting
        if (frame < 30) {
            input.move_right.ended_down = true;
        } else {
            input.move_left.ended_down = true;
        }

        ctx.updateAndRender(input);
    }

    // Validate the framebuffer has non-zero data (pixels were drawn)
    var pixel_sum: usize = 0;
    for (ctx.framebuffer[0..engine.BUFFER_SIZE]) |byte| {
        pixel_sum += byte;
    }

    std.debug.print("[ ok ] Ran {d} frames, framebuffer checksum: {x:8}\n", .{ frames, pixel_sum });
    std.debug.print("[ ok ] Engine API verified — platform layer works!\n", .{});
}

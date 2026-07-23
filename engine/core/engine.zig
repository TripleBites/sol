const std = @import("std");

// ── Display constants ────────────────────────────────────────────────────────
pub const WIDTH: u32 = 320;
pub const HEIGHT: u32 = 240;
pub const BUFFER_SIZE: usize = @as(usize, WIDTH * HEIGHT * 4);

// ── Input (Handmade Hero style) ──────────────────────────────────────────────
/// A single button — the game reads `ended_down` for held state and
/// `half_transition_count` to detect presses / releases.
pub const Button = struct {
    ended_down: bool = false,
    half_transition_count: u32 = 0,
};

pub const EngineInput = struct {
    // Keyboard
    move_up: Button = .{},
    move_down: Button = .{},
    move_left: Button = .{},
    move_right: Button = .{},
    action1: Button = .{},
    action2: Button = .{},

    // Mouse
    mouse_x: i32 = 0,
    mouse_y: i32 = 0,
    mouse_buttons: [3]Button = [_]Button{ .{}, .{}, .{} },

    // Timing (seconds since the last frame)
    dt_for_frame: f32 = 1.0 / 60.0,
};

// ── Engine / Game State ──────────────────────────────────────────────────────
pub const EngineContext = struct {
    framebuffer: [BUFFER_SIZE]u8 = undefined,

    // Game state (this is the "game" part — replace with your own later)
    player_x: f32 = 100.0,
    player_y: f32 = 100.0,
    velocity_x: f32 = 60.0,
    velocity_y: f32 = 80.0,

    // Previous frame's input — used for transition detection
    prev_input: EngineInput = .{},

    // ── Initialisation ────────────────────────────────────────────────────
    pub fn init(self: *EngineContext) void {
        self.clearScreen(0xFF1E1E1E);
    }

    // ── Per-frame update + render ─────────────────────────────────────────
    /// The platform layer calls this exactly once per frame with the
    /// latest input snapshot.  The engine writes its pixels into
    /// `self.framebuffer` and the platform is responsible for displaying them.
    pub fn updateAndRender(self: *EngineContext, input: EngineInput) void {
        // ── Process button transitions ──
        //  (Handmade Hero style — the game can see exactly what happened)
        const pressed_up = wasPressed(&input.move_up, &self.prev_input.move_up);
        const pressed_down = wasPressed(&input.move_down, &self.prev_input.move_down);
        const pressed_left = wasPressed(&input.move_left, &self.prev_input.move_left);
        const pressed_right = wasPressed(&input.move_right, &self.prev_input.move_right);
        const pressed_action1 = wasPressed(&input.action1, &self.prev_input.action1);
        _ = pressed_up;
        _ = pressed_down;
        _ = pressed_left;
        _ = pressed_right;
        _ = pressed_action1;

        // ── Movement with held keys ──
        if (input.move_up.ended_down) {
            self.velocity_y = -80.0;
        } else if (input.move_down.ended_down) {
            self.velocity_y = 80.0;
        } else {
            self.velocity_y = 0;
        }

        if (input.move_left.ended_down) {
            self.velocity_x = -60.0;
        } else if (input.move_right.ended_down) {
            self.velocity_x = 60.0;
        } else {
            self.velocity_x = 0;
        }

        // ── Physics update ──
        self.player_x += self.velocity_x * input.dt_for_frame;
        self.player_y += self.velocity_y * input.dt_for_frame;

        // Bounce / clamp
        const max_x: f32 = @floatFromInt(WIDTH - 16);
        const max_y: f32 = @floatFromInt(HEIGHT - 16);

        if (self.player_x < 0) {
            self.player_x = 0;
            self.velocity_x = 60.0;
        } else if (self.player_x > max_x) {
            self.player_x = max_x;
            self.velocity_x = -60.0;
        }
        if (self.player_y < 0) {
            self.player_y = 0;
            self.velocity_y = 80.0;
        } else if (self.player_y > max_y) {
            self.player_y = max_y;
            self.velocity_y = -80.0;
        }

        // ── Render ──
        self.clearScreen(0xFF1E1E1E);

        // Draw a moving player square
        self.drawRect(
            @intFromFloat(self.player_x),
            @intFromFloat(self.player_y),
            16,
            16,
            0xFF00FFFF, // cyan (RGBA in little-endian → B, G, R, A in bytes)
        );

        // ── Save input for next frame ──
        self.prev_input = input;
    }

    // ── Internal helpers ──────────────────────────────────────────────────
    fn clearScreen(self: *EngineContext, color: u32) void {
        const r: u8 = @truncate(color >> 0);
        const g: u8 = @truncate(color >> 8);
        const b: u8 = @truncate(color >> 16);
        const a: u8 = @truncate(color >> 24);

        @memset(self.framebuffer[0..BUFFER_SIZE], 0);
        var i: usize = 0;
        while (i < BUFFER_SIZE) : (i += 4) {
            self.framebuffer[i + 0] = r;
            self.framebuffer[i + 1] = g;
            self.framebuffer[i + 2] = b;
            self.framebuffer[i + 3] = a;
        }
    }

    fn drawRect(self: *EngineContext, x: usize, y: usize, w: usize, h: usize, color: u32) void {
        const r: u8 = @truncate(color >> 0);
        const g: u8 = @truncate(color >> 8);
        const b: u8 = @truncate(color >> 16);
        const a: u8 = @truncate(color >> 24);

        var row: usize = 0;
        while (row < h) : (row += 1) {
            const py = y + row;
            if (py >= HEIGHT) continue;
            var col: usize = 0;
            while (col < w) : (col += 1) {
                const px = x + col;
                if (px >= WIDTH) continue;
                const idx = (py * @as(usize, WIDTH) + px) * 4;
                self.framebuffer[idx + 0] = r;
                self.framebuffer[idx + 1] = g;
                self.framebuffer[idx + 2] = b;
                self.framebuffer[idx + 3] = a;
            }
        }
    }
};

// ── Utility ──────────────────────────────────────────────────────────────────
/// Returns true if the button went *down* this frame.
/// (Handmade Hero's "just pressed" test via half_transition_count.)
pub fn wasPressed(btn: *const Button, prev: *const Button) bool {
    return btn.ended_down and !prev.ended_down;
}

/// Returns true if the button went *up* this frame.
pub fn wasReleased(btn: *const Button, prev: *const Button) bool {
    return !btn.ended_down and prev.ended_down;
}

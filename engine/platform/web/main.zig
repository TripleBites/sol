const std = @import("std");

// Functions provided by JavaScript
extern "env" fn drawRect(x: f32, y: f32, w: f32, h: f32) void;
extern "env" fn clearScreen() void;

var player_x: f32 = 100.0;
var player_y: f32 = 100.0;

// Exported functions called by JavaScript's requestAnimationFrame
export fn update(dt: f32, move_right: bool, move_left: bool) void {
    const speed: f32 = 200.0;
    if (move_right) player_x += speed * dt;
    if (move_left) player_x -= speed * dt;
}

export fn render() void {
    clearScreen();
    drawRect(player_x, player_y, 40.0, 40.0);
}

// Override panic handler for freestanding targets
pub fn panic(msg: []const u8, trace: ?*std.builtin.StackTrace, ret_addr: ?usize) noreturn {
    _ = msg;
    _ = trace;
    _ = ret_addr;
    @trap();
}

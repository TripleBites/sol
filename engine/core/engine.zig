const std = @import("std");
const pocketpy = @import("pocketpy");
const Io = std.Io;

pub const WIDTH: usize = 320;
pub const HEIGHT: usize = 240;
pub const BUFFER_SIZE: usize = WIDTH * HEIGHT * 4; // 4 bytes per pixel (RGBA)

pub const GameState = struct {
    player_x: f32 = 100.0,
    player_y: f32 = 100.0,
    velocity_x: f32 = 60.0,
    velocity_y: f32 = 80.0,
    // The raw pixel buffer that the platform layer will display
    framebuffer: [BUFFER_SIZE]u8 = undefined,

    pub fn init(self: *GameState) void {
        // Initialize screen to dark gray
        self.clearScreen(0xFF1E1E1E);
    }

    pub fn update(self: *GameState, delta_time: f32) void {
        // Move player
        self.player_x += self.velocity_x * delta_time;
        self.player_y += self.velocity_y * delta_time;

        // Bounce off screen bounds
        if (self.player_x < 0 or self.player_x > @as(f32, @floatFromInt(WIDTH - 16))) {
            self.velocity_x *= -1.0;
        }
        if (self.player_y < 0 or self.player_y > @as(f32, @floatFromInt(HEIGHT - 16))) {
            self.velocity_y *= -1.0;
        }

        // Render frame
        self.clearScreen(0xFF1E1E1E);
        self.drawRect(
            @as(usize, @intFromFloat(self.player_x)),
            @as(usize, @intFromFloat(self.player_y)),
            16,
            16,
            0xFF00FFFF, // Yellow box in ABGR memory layout (RGBA in little-endian)
        );
    }

    fn clearScreen(self: *GameState, color: u32) void {
        var i: usize = 0;
        while (i < BUFFER_SIZE) : (i += 4) {
            self.framebuffer[i + 0] = @truncate(color >> 0); // R
            self.framebuffer[i + 1] = @truncate(color >> 8); // G
            self.framebuffer[i + 2] = @truncate(color >> 16); // B
            self.framebuffer[i + 3] = @truncate(color >> 24); // A
        }
    }

    fn drawRect(self: *GameState, x: usize, y: usize, w: usize, h: usize, color: u32) void {
        var row: usize = 0;
        while (row < h) : (row += 1) {
            const py = y + row;
            if (py >= HEIGHT) continue;

            var col: usize = 0;
            while (col < w) : (col += 1) {
                const px = x + col;
                if (px >= WIDTH) continue;

                const index = (py * WIDTH + px) * 4;
                self.framebuffer[index + 0] = @truncate(color >> 0);
                self.framebuffer[index + 1] = @truncate(color >> 8);
                self.framebuffer[index + 2] = @truncate(color >> 16);
                self.framebuffer[index + 3] = @truncate(color >> 24);
            }
        }
    }
};

/// Sol Game Engine — Entry Point
///
/// Bootstraps the engine subsystems (pocketpy, graphics window, audio) and
/// runs a minimal smoke test to verify every library is linked and callable.
// pub fn main(init: std.process.Init) !void {
//     //const io = init.io;
//     const arena = init.arena.allocator();
//     const args = try init.minimal.args.toSlice(arena);
//     _ = args;

//     // ---- stdout ----
//     var stdout_buf: [1024]u8 = undefined;
//     var stdout_fw: Io.File.Writer = .init(.stdout(), init.io, &stdout_buf);
//     const stdout = &stdout_fw.interface;

//     // ---- banner ----
//     try stdout.print(
//         \\
//         \\╔══════════════════════════════════════╗
//         \\║   Sol Game Engine v0.1.0-dev         ║
//         \\║   Powered by Zig + pocketpy          ║
//         \\╚══════════════════════════════════════╝
//         \\
//         \\
//     , .{});

//     const builtin = @import("builtin");
//     try stdout.print("[info] Zig version:    {s}\n", .{builtin.zig_version_string});
//     try stdout.print("[info] Target:         {s}-{s}-{s}\n", .{
//         @tagName(builtin.target.cpu.arch),
//         @tagName(builtin.target.os.tag),
//         @tagName(builtin.target.abi),
//     });
//     try stdout.print("[info] Optimize:       {s}\n", .{@tagName(builtin.mode)});
//     try stdout.print("[info] pocketpy:       {s}\n\n", .{pocketpy.versionString()});

//     // ---- pocketpy smoke test ----
//     try stdout.print("--- pocketpy smoke test ---\n", .{});
//     try stdout.flush();

//     pocketpy.initialize();
//     defer pocketpy.finalize();

//     const py_ok = pocketpy.exec(
//         \\print("Hello from pocketpy inside Sol Engine! 🐍")
//     );
//     if (!py_ok) {
//         try stdout.print("[warn] Python script failed\n", .{});
//     }
//     try stdout.flush();

//     try stdout.print("\n[ ok ] Engine bootstrap complete.\n", .{});
//     try stdout.flush();
// }

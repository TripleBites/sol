//! Sol Engine — Graphics / Windowing subsystem.
//!
//! Wraps GLFW for window creation, input polling, and swap-chain
//! presentation.  All GLFW interaction lives behind this module so
//! the rest of the engine never imports zglfw directly.

const std = @import("std");
const glfw = @import("zglfw");

/// Opaque window handle.  Created via `Window.create`, destroyed via
/// `window.destroy()`.
pub const Window = struct {
    handle: *glfw.Window,

    /// Open a new window.  Calls glfwInit on first use.
    pub fn create(width: u32, height: u32, title: [:0]const u8) !Window {
        std.debug.print("[info] window:         creating {d}x{d} \"{s}\"...\n", .{ width, height, title });

        const handle = try glfw.Window.create(
            @intCast(width),
            @intCast(height),
            title,
            null, // monitor
            null, // share
        );

        return Window{ .handle = handle };
    }

    /// True when the user has clicked the close button.
    pub fn shouldClose(self: Window) bool {
        return self.handle.shouldClose();
    }

    /// Swap the front and back buffers.
    pub fn swapBuffers(self: Window) void {
        self.handle.swapBuffers();
    }

    /// Destroy the window and free associated resources.
    pub fn destroy(self: Window) void {
        self.handle.destroy();
    }
};

// -------------------------------------------------------------------
// Library lifecycle
// -------------------------------------------------------------------

/// Initialise GLFW.  Safe to call multiple times (idempotent inside
/// GLFW).  Must be called before any Window is created.
pub fn init() !void {
    std.debug.print("[info] window:         initializing GLFW...\n", .{});
    try glfw.init();
}

/// Shut down GLFW.  Call once at engine exit.
pub fn terminate() void {
    glfw.terminate();
}

/// Poll for queued input events.  Call every frame.
pub fn pollEvents() void {
    glfw.pollEvents();
}

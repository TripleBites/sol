//! Sol Engine — Audio subsystem.
//!
//! Wraps zaudio / miniaudio for engine-level sound playback.  All audio
//! interaction lives behind this module so the rest of the engine never
//! imports zaudio directly.

const std = @import("std");
const zaudio = @import("zaudio");

/// Opaque audio engine handle.
pub const Engine = struct {
    handle: *zaudio.Engine,

    /// Create the audio engine.  Must be called after `init()`.
    pub fn create(config: ?zaudio.Engine.Config) !Engine {
        std.debug.print("[info] audio:          creating engine...\n", .{});
        const handle = try zaudio.Engine.create(config);
        return Engine{ .handle = handle };
    }

    /// Shut down the audio engine.
    pub fn destroy(self: Engine) void {
        self.handle.destroy();
    }
};

// -------------------------------------------------------------------
// Library lifecycle
// -------------------------------------------------------------------

/// Initialise zaudio.  Must be called once before `Engine.create`.
pub fn init(allocator: std.mem.Allocator) void {
    std.debug.print("[info] audio:          initializing...\n", .{});
    zaudio.init(allocator);
}

/// De-initialise zaudio.  Call once at engine exit.
pub fn deinit() void {
    zaudio.deinit();
}

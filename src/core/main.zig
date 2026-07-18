const std = @import("std");
const Io = std.Io;
const pocketpy = @import("pocketpy");
const window = @import("graphics/window.zig");
const audio = @import("audio/audio.zig");

/// Sol Game Engine — Entry Point
///
/// Bootstraps the engine subsystems (pocketpy, graphics window, audio) and
/// runs a minimal smoke test to verify every library is linked and callable.
pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    const args = try init.minimal.args.toSlice(arena);
    _ = args;

    // ---- stdout ----
    var stdout_buf: [1024]u8 = undefined;
    var stdout_fw: Io.File.Writer = .init(.stdout(), init.io, &stdout_buf);
    const stdout = &stdout_fw.interface;

    // ---- banner ----
    try stdout.print(
        \\
        \\╔══════════════════════════════════════╗
        \\║   Sol Game Engine v0.1.0-dev         ║
        \\║   Powered by Zig + pocketpy          ║
        \\╚══════════════════════════════════════╝
        \\
        \\
    , .{});

    const builtin = @import("builtin");
    try stdout.print("[info] Zig version:    {s}\n", .{builtin.zig_version_string});
    try stdout.print("[info] Target:         {s}-{s}-{s}\n", .{
        @tagName(builtin.target.cpu.arch),
        @tagName(builtin.target.os.tag),
        @tagName(builtin.target.abi),
    });
    try stdout.print("[info] Optimize:       {s}\n", .{@tagName(builtin.mode)});
    try stdout.print("[info] pocketpy:       {s}\n\n", .{pocketpy.versionString()});

    // ---- pocketpy smoke test ----
    try stdout.print("--- pocketpy smoke test ---\n", .{});
    try stdout.flush();

    pocketpy.initialize();
    defer pocketpy.finalize();

    const py_ok = pocketpy.exec(
        \\print("Hello from pocketpy inside Sol Engine! 🐍")
    );
    if (!py_ok) {
        try stdout.print("[warn] Python script failed\n", .{});
    }
    try stdout.flush();

    // ---- graphics window ----
    try window.init();
    defer window.terminate();

    const win = try window.Window.create(600, 600, "Sol Engine — Smoke Test");
    defer win.destroy();

    // ---- audio engine ----
    audio.init(arena);
    defer audio.deinit();

    const engine = try audio.Engine.create(null);
    defer engine.destroy();

    try stdout.flush();

    // ---- main loop (close window to exit) ----
    while (!win.shouldClose()) {
        window.pollEvents();
        win.swapBuffers();
    }

    try stdout.print("\n[ ok ] Engine bootstrap complete.\n", .{});
    try stdout.flush();
}

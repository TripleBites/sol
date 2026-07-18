const std = @import("std");
const glfw = @import("zglfw");
const zaudio = @import("zaudio");
const Io = std.Io;
const pocketpy = @import("pocketpy");

/// Sol Game Engine — Entry Point
///
/// Bootstraps the engine and runs a minimal pocketpy hello-world script to
/// verify that the Python interpreter is linked and callable.
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

    // ---- Hello-world Python via pocketpy ----
    try stdout.print("--- pocketpy smoke test ---\n", .{});

    pocketpy.initialize();
    defer pocketpy.finalize();

    const py_ok = pocketpy.exec(
        \\print("Hello from pocketpy inside Sol Engine! 🐍")
    );
    if (!py_ok) {
        try stdout.print("[warn] Python script failed\n", .{});
    }

    // Flush all output before touching GLFW (buffered stdout + C stdout).
    try stdout.flush();

    // ---- GLFW window ----
    try stdout.print("[info] glfw:           initializing...\n", .{});
    try stdout.flush();

    // Wayland backend is disabled at build time (see build.zig).
    // Only X11 is compiled in, so the platform hint is redundant but safe.

    try glfw.init();
    defer glfw.terminate();

    try stdout.print("[info] glfw:           creating window...\n", .{});
    try stdout.flush();

    const window = try glfw.Window.create(600, 600, "Sol Engine — Smoke Test", null, null);
    defer window.destroy();

    // ---- zaudio engine ----
    zaudio.init(arena);
    defer zaudio.deinit();

    const engine = try zaudio.Engine.create(null);
    defer engine.destroy();

    // Flush buffered output before entering the render loop.
    try stdout.flush();

    // ---- main loop (close window to exit) ----
    while (!window.shouldClose()) {
        glfw.pollEvents();
        window.swapBuffers();
    }

    try stdout.print("\n[ ok ] Engine bootstrap complete.\n", .{});

    // Always flush before exit.
    try stdout.flush();
}

const std = @import("std");

/// Build the Sol Engine web target.
///
/// Called from the root `build.zig` when `-Dplatform=web` is selected.
/// `engine_module` is the Zig module that provides `engine` (engine/core/engine.zig).
pub fn buildWeb(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const activate_emsdk_step = @import("zemscripten").activateEmsdkStep(b);

    // ── Web platform module (main.zig) ────────────────────────────────────
    const wasm = b.addLibrary(.{
        .name = "sol",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .root_source_file = b.path("main.zig"),
            .imports = &.{
                .{ .name = "engine", .module = engine_module },
            },
        }),
    });

    // zemscripten import
    const zemscripten = b.dependency("zemscripten", .{});
    wasm.root_module.addImport("zemscripten", zemscripten.module("root"));

    // ── Emscripten linker step ────────────────────────────────────────────
    const emcc_flags = @import("zemscripten").emccDefaultFlags(b.allocator, .{
        .optimize = optimize,
        .fsanitize = false,
    });

    var emcc_settings = @import("zemscripten").emccDefaultSettings(b.allocator, .{
        .optimize = optimize,
        .emsdk_allocator = .emmalloc,
    });
    emcc_settings.put("ALLOW_MEMORY_GROWTH", "1") catch unreachable;

    const emcc_step = @import("zemscripten").emccStep(
        b,
        &.{}, // extra source files
        &.{wasm},
        .{
            .optimize = optimize,
            .flags = emcc_flags,
            .settings = emcc_settings,
            .use_preload_plugins = false,
            .embed_paths = &.{},
            .preload_paths = &.{},
            .shell_file_path = b.path("shell.html"),
            .js_library_path = b.path("canvas.js"),
            .out_file_name = "sol.html",
            .install_dir = .{ .custom = "web" },
        },
    );
    emcc_step.dependOn(activate_emsdk_step);

    b.getInstallStep().dependOn(emcc_step);

    // ── emrun step (for local dev) ────────────────────────────────────────
    const emrun_step = @import("zemscripten").emrunStep(
        b,
        b.fmt("zig-out/web/sol.html", .{}),
        &.{},
    );
    emrun_step.dependOn(emcc_step);

    b.step("emrun", "Build and open the web app locally using emrun").dependOn(emrun_step);
}

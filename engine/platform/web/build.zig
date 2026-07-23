const std = @import("std");

/// Build the Sol Engine web target using the system-installed Emscripten.
///
/// Called from root `build.zig` when `-Dplatform=web`.
/// Relies on `emcc` being available on PATH.
pub fn buildWeb(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    _ = target; // we use our own wasm32-emscripten target below

    // ── Locate system emcc ────────────────────────────────────────────────
    const emcc_path = findEmcc();

    // Zemscripten build helpers (flags, settings — not the runtime module)
    const zb = @import("zemscripten");

    // ── Web platform static library ───────────────────────────────────────
    //  Compiled for wasm32-emscripten so emcc can link the LLVM bitcode.
    const wasm_target = b.resolveTargetQuery(.{
        .cpu_arch = .wasm32,
        .os_tag = .emscripten,
    });
    const web_module = b.createModule(.{
        .target = wasm_target,
        .optimize = optimize,
        .root_source_file = b.path("engine/platform/web/main.zig"),
        .imports = &.{
            .{ .name = "engine", .module = engine_module },
        },
    });

    // Zemscripten runtime module (setMainLoop, panic, log)
    const zemscripten = b.dependency("zemscripten", .{});
    web_module.addImport("zemscripten", zemscripten.module("root"));

    const wasm = b.addLibrary(.{
        .name = "sol",
        .linkage = .static,
        .root_module = web_module,
    });

    // ── Build emcc invocation ─────────────────────────────────────────────
    var emcc = b.addSystemCommand(&.{emcc_path});

    // Flags
    var flags = zb.emccDefaultFlags(b.allocator, .{
        .optimize = optimize,
        .fsanitize = false,
    });
    var flags_iter = flags.iterator();
    while (flags_iter.next()) |kvp| {
        emcc.addArg(kvp.key_ptr.*);
    }

    // Settings (-s KEY=VAL)
    var settings = zb.emccDefaultSettings(b.allocator, .{
        .optimize = optimize,
        .emsdk_allocator = .emmalloc,
    });
    settings.put("ALLOW_MEMORY_GROWTH", "1") catch unreachable;
    var settings_iter = settings.iterator();
    while (settings_iter.next()) |kvp| {
        emcc.addArg(b.fmt("-s{s}={s}", .{ kvp.key_ptr.*, kvp.value_ptr.* }));
    }

    // Input: the Zig static library
    emcc.addArtifactArg(wasm);

    // Output
    emcc.addArg("-o");
    const out_file = emcc.addOutputFileArg("sol.html");

    // Shell file
    emcc.addArg("--shell-file");
    emcc.addFileArg(b.path("engine/platform/web/shell.html"));

    // JS library (canvas blit + input tracking)
    emcc.addArg("--js-library");
    emcc.addFileArg(b.path("engine/platform/web/canvas.js"));

    // ── Install the output ────────────────────────────────────────────────
    const install_step = b.addInstallDirectory(.{
        .source_dir = out_file.dirname(),
        .install_dir = .{ .custom = "web" },
        .install_subdir = "",
    });
    install_step.step.dependOn(&emcc.step);

    b.getInstallStep().dependOn(&install_step.step);

    // ── emrun step (optional) ─────────────────────────────────────────────
    const emrun = b.addSystemCommand(&.{ "emrun" });
    emrun.addArg("--no_browser");
    emrun.addArg("--port");
    emrun.addArg("8080");
    emrun.addFileArg(out_file);
    emrun.step.dependOn(&install_step.step);

    b.step("emrun", "Serve locally via emrun on port 8080").dependOn(&emrun.step);
}

/// Path to the system `emcc` binary.
fn findEmcc() []const u8 {
    return "/data/data/com.termux/files/usr/opt/emscripten/emcc";
}

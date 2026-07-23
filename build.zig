const std = @import("std");
const pocketpy_build = @import("engine/core/pocketpy/build.zig");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // ── Platform selection ────────────────────────────────────────────────
    //  Usage:  zig build -Dplatform=web
    //          zig build -Dplatform=sdl3
    //          zig build -Dplatform=termux
    //          zig build -Dplatform=windows
    //          zig build -Dplatform=native   (default: native desktop)
    const platform = b.option([]const u8, "platform", "Target platform [web, sdl3, termux, windows, native]") orelse "native";

    // ── Engine core module ────────────────────────────────────────────────
    //  This is the pure game/engine code that all platform layers share.
    //  It knows nothing about the OS, window, or input handling.
    const engine_module = b.createModule(.{
        .root_source_file = b.path("engine/core/engine.zig"),
        .target = target,
        .optimize = optimize,
    });

    // ── PocketPy (optional scripting — built separately, not in engine) ──
    //    The engine does not depend on pocketpy; uncomment when the C
    //    toolchain issue on this host is resolved (Termux/Android missing
    //    asm/types.h).
    // _ = buildPocketpy(b, engine_module, target, optimize);

    // ── Dispatch to platform builder ──────────────────────────────────────
    if (std.mem.eql(u8, platform, "web")) {
        buildWeb(b, engine_module, target, optimize);
    } else if (std.mem.eql(u8, platform, "sdl3")) {
        buildSdl3(b, engine_module, target, optimize);
    } else if (std.mem.eql(u8, platform, "termux")) {
        buildTermux(b, engine_module, target, optimize);
    } else if (std.mem.eql(u8, platform, "windows")) {
        buildWindows(b, engine_module, target, optimize);
    } else if (std.mem.eql(u8, platform, "native")) {
        // Default native build — uses SDL3 if available, otherwise termux
        buildSdl3(b, engine_module, target, optimize);
    } else {
        std.debug.print("Sol: unknown platform '{s}'\n", .{platform});
        std.process.exit(1);
    }
}

// ── Web (WASM / Emscripten) ──────────────────────────────────────────────────
fn buildWeb(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    const web_build = @import("engine/platform/web/build.zig");
    web_build.buildWeb(b, engine_module, target, optimize);
}

// ── SDL3 (desktop / native) ──────────────────────────────────────────────────
fn buildSdl3(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    // TODO: implement SDL3 platform layer
    // const sdl3 = @import("engine/platform/sdl3/build.zig");
    // sdl3.buildSdl3(b, engine_module, target, optimize);

    // For now, just build and run the engine module as a test
    buildStandalone(b, engine_module, target, optimize, "sol-sdl3");
}

// ── Termux (Android terminal) ────────────────────────────────────────────────
fn buildTermux(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    _ = b;
    _ = engine_module;
    _ = target;
    _ = optimize;

    // TODO: implement Termux platform layer
    // const termux = @import("engine/platform/termux/build.zig");
    // termux.buildTermux(b, engine_module, target, optimize);
}

// ── Windows (Win32) ──────────────────────────────────────────────────────────
fn buildWindows(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) void {
    _ = b;
    _ = engine_module;
    _ = target;
    _ = optimize;

    // TODO: implement Windows platform layer
    // const win = @import("engine/platform/windows/build.zig");
    // win.buildWindows(b, engine_module, target, optimize);
}

// ── Standalone (for platforms not yet implemented) ───────────────────────────
fn buildStandalone(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
    name: []const u8,
) void {
    const exe = b.addExecutable(.{
        .name = name,
        .root_module = b.createModule(.{
            .root_source_file = b.path("engine/platform/native/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                .{ .name = "engine", .module = engine_module },
            },
        }),
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", b.fmt("Run {s}", .{name}));
    run_step.dependOn(&run_cmd.step);
}

// ── Optional pocketpy scripting ──────────────────────────────────────────────
fn buildPocketpy(
    b: *std.Build,
    engine_module: *std.Build.Module,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) bool {
    // Skip pocketpy when the C compiler can't target the host (Termux/Android)
    const pk = pocketpy_build.buildPocketpy(b, target, optimize) catch |err| {
        std.debug.print("Sol: pocketpy build skipped (no C toolchain) — {}\n", .{err});
        return false;
    };
    engine_module.addImport("pocketpy", pk.module);
    engine_module.linkLibrary(pk.library);
    return true;
}

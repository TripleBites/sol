const std = @import("std");

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const activate_emsdk_step = @import("zemscripten").activateEmsdkStep(b);

    const wasm = b.addLibrary(.{
        .name = "sol",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .root_source_file = b.path("main.zig"),
        }),
    });

    const zemscripten = b.dependency("zemscripten", .{});
    wasm.root_module.addImport("zemscripten", zemscripten.module("root"));

    const emcc_flags = @import("zemscripten").emccDefaultFlags(b.allocator, .{
        .optimize = optimize,
        .fsanitize = false,
    });

    var emcc_settings = @import("zemscripten").emccDefaultSettings(b.allocator, .{
        .optimize = optimize,
        .emsdk_allocator = .emmalloc,
    });
    emcc_settings.put("ALLOW_MEMORY_GROWTH", "1") catch unreachable;
    emcc_settings.put("FULL_ES2", "1") catch unreachable;

    const emcc_step = @import("zemscripten").emccStep(
        b,
        &.{}, // src file paths
        &.{wasm}, // src compile steps
        .{
            .optimize = optimize,
            .flags = emcc_flags,
            .settings = emcc_settings,
            .use_preload_plugins = true,
            .embed_paths = &.{},
            .preload_paths = &.{},
            .shell_file_path = b.path("shell.html"),
            .js_library_path = null,
            .out_file_name = "sol.html", // or "sol.js"
            .install_dir = .{ .custom = "web" },
        },
    );
    emcc_step.dependOn(activate_emsdk_step);

    b.getInstallStep().dependOn(emcc_step);

    const html_filename = try std.fmt.allocPrint(b.allocator, "{s}.html", .{wasm.name});

    const emrun_args = .{};
    const emrun_step = @import("zemscripten").emrunStep(
        b,
        b.path(b.fmt("zig-out/web/{s}", .{html_filename})),
        &emrun_args,
    );

    emrun_step.dependOn(emcc_step);

    b.step("emrun", "Build and open the web app locally using emrun").dependOn(emrun_step);
}

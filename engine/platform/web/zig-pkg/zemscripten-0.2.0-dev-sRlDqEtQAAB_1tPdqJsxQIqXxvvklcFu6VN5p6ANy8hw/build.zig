const builtin = @import("builtin");
const std = @import("std");

pub const emsdk_ver_major = "4";
pub const emsdk_ver_minor = "0";
pub const emsdk_ver_tiny = "19";
pub const emsdk_version = emsdk_ver_major ++ "." ++ emsdk_ver_minor ++ "." ++ emsdk_ver_tiny;

pub fn build(b: *std.Build) void {
    _ = b.addModule("root", .{ .root_source_file = b.path("src/zemscripten.zig") });
}

/// Returns a LazyPath to the emsdk dependency root joined with the given sub_paths.
fn emsdkPath(b: *std.Build, sub_paths: []const []const u8) std.Build.LazyPath {
    return b.dependency("emsdk", .{}).path(b.pathJoin(sub_paths));
}

pub fn emccPath(b: *std.Build) std.Build.LazyPath {
    return emsdkPath(b, &.{ "upstream", "emscripten", "emcc.py" });
}

pub fn emrunPath(b: *std.Build) std.Build.LazyPath {
    const tool = switch (builtin.target.os.tag) {
        .windows => "emrun.bat",
        else => "emrun",
    };
    return emsdkPath(b, &.{ "upstream", "emscripten", tool });
}

pub fn htmlPath(b: *std.Build) std.Build.LazyPath {
    return emsdkPath(b, &.{ "upstream", "emscripten", "src", "shell.html" });
}

pub fn activateEmsdkStep(b: *std.Build) *std.Build.Step {
    const emsdk_script = switch (builtin.target.os.tag) {
        .windows => emsdkPath(b, &.{"emsdk.bat"}),
        else => emsdkPath(b, &.{"emsdk"}),
    };

    var emsdk_update = b.addRunFile(emsdk_script);
    emsdk_update.addArg("update");

    var emsdk_install = b.addRunFile(emsdk_script);
    emsdk_install.addArgs(&.{ "install", emsdk_version });
    emsdk_install.step.dependOn(&emsdk_update.step);

    switch (builtin.target.os.tag) {
        .linux, .macos => {
            const chmod_script = b.addSystemCommand(&.{ "chmod", "+x" });
            chmod_script.addFileArg(emsdk_script);
            emsdk_install.step.dependOn(&chmod_script.step);
        },
        .windows => {
            const takeown_script = b.addSystemCommand(&.{ "takeown", "/f" });
            takeown_script.addFileArg(emsdk_script);
            emsdk_install.step.dependOn(&takeown_script.step);
        },
        else => {},
    }

    var emsdk_activate = b.addRunFile(emsdk_script);
    emsdk_activate.addArgs(&.{ "activate", emsdk_version });
    emsdk_activate.step.dependOn(&emsdk_install.step);

    const top_level = b.allocator.create(std.Build.Step.TopLevel) catch unreachable;
    top_level.* = .{
        .step = std.Build.Step.init(.{
            .tag = .top_level,
            .name = "Activate EMSDK",
            .owner = b,
        }),
        .description = "Activate Emscripten SDK",
    };

    switch (builtin.target.os.tag) {
        .linux, .macos => {
            const chmod_emcc = b.addSystemCommand(&.{ "chmod", "a+x" });
            chmod_emcc.addFileArg(emccPath(b));
            chmod_emcc.step.dependOn(&emsdk_activate.step);
            top_level.step.dependOn(&chmod_emcc.step);

            const chmod_emrun = b.addSystemCommand(&.{ "chmod", "a+x" });
            chmod_emrun.addFileArg(emrunPath(b));
            chmod_emrun.step.dependOn(&emsdk_activate.step);
            top_level.step.dependOn(&chmod_emrun.step);
        },
        .windows => {
            const takeown_emcc = b.addSystemCommand(&.{ "takeown", "/f" });
            takeown_emcc.addFileArg(emccPath(b));
            takeown_emcc.step.dependOn(&emsdk_activate.step);
            top_level.step.dependOn(&takeown_emcc.step);

            const takeown_emrun = b.addSystemCommand(&.{ "takeown", "/f" });
            takeown_emrun.addFileArg(emrunPath(b));
            takeown_emrun.step.dependOn(&emsdk_activate.step);
            top_level.step.dependOn(&takeown_emrun.step);
        },
        else => {},
    }

    return &top_level.step;
}

pub const EmccFlags = std.StringHashMap(void);

pub const EmccDefaultFlagsOverrides = struct {
    optimize: std.builtin.OptimizeMode,
    fsanitize: bool,
};

pub fn emccDefaultFlags(allocator: std.mem.Allocator, options: EmccDefaultFlagsOverrides) EmccFlags {
    var args = EmccFlags.init(allocator);
    switch (options.optimize) {
        .Debug => {
            args.put("-O0", {}) catch unreachable;
            args.put("-gsource-map", {}) catch unreachable;
            if (options.fsanitize)
                args.put("-fsanitize=undefined", {}) catch unreachable;
        },
        .ReleaseSafe => {
            args.put("-O3", {}) catch unreachable;
            if (options.fsanitize) {
                args.put("-fsanitize=undefined", {}) catch unreachable;
                args.put("-fsanitize-minimal-runtime", {}) catch unreachable;
            }
        },
        .ReleaseFast => {
            args.put("-O3", {}) catch unreachable;
        },
        .ReleaseSmall => {
            args.put("-Oz", {}) catch unreachable;
        },
    }
    return args;
}

pub const EmccSettings = std.StringHashMap([]const u8);

pub const EmsdkAllocator = enum {
    none,
    dlmalloc,
    emmalloc,
    @"emmalloc-debug",
    @"emmalloc-memvalidate",
    @"emmalloc-verbose",
    mimalloc,
};

pub const EmccDefaultSettingsOverrides = struct {
    optimize: std.builtin.OptimizeMode,
    emsdk_allocator: EmsdkAllocator = .emmalloc,
};

pub fn emccDefaultSettings(allocator: std.mem.Allocator, options: EmccDefaultSettingsOverrides) EmccSettings {
    var settings = EmccSettings.init(allocator);
    switch (options.optimize) {
        .Debug, .ReleaseSafe => {
            settings.put("SAFE_HEAP", "1") catch unreachable;
            settings.put("STACK_OVERFLOW_CHECK", "1") catch unreachable;
            settings.put("ASSERTIONS", "1") catch unreachable;
        },
        else => {},
    }
    settings.put("MALLOC", @tagName(options.emsdk_allocator)) catch unreachable;
    return settings;
}

pub const ResourceFile = struct {
    src_path: std.Build.LazyPath,
    virtual_path: ?[]const u8 = null,

    pub fn get(self: ResourceFile, b: *std.Build) []const u8 {
        return if (self.virtual_path) |virtual_path|
            b.fmt(
                "{s}@{s}",
                .{ self.src_path.getPath(b), virtual_path },
            )
        else
            self.src_path.getPath(b);
    }
};

pub const StepOptions = struct {
    optimize: std.builtin.OptimizeMode,
    flags: EmccFlags,
    settings: EmccSettings,
    use_preload_plugins: bool = false,
    embed_paths: ?[]const ResourceFile = null,
    preload_paths: ?[]const ResourceFile = null,
    shell_file_path: ?std.Build.LazyPath = null,
    js_library_path: ?std.Build.LazyPath = null,
    out_file_name: []const u8,
    install_dir: std.Build.InstallDir,
};

pub fn emccStep(
    b: *std.Build,
    src_paths: []const std.Build.LazyPath,
    compile_steps: []const *std.Build.Step.Compile,
    options: StepOptions,
) *std.Build.Step {
    var emcc = b.addRunFile(emccPath(b));

    var iterFlags = options.flags.iterator();
    while (iterFlags.next()) |kvp| {
        emcc.addArg(kvp.key_ptr.*);
    }

    var iterSettings = options.settings.iterator();
    while (iterSettings.next()) |kvp| {
        emcc.addArg(std.fmt.allocPrint(
            b.allocator,
            "-s{s}={s}",
            .{ kvp.key_ptr.*, kvp.value_ptr.* },
        ) catch unreachable);
    }

    for (src_paths) |src_path| {
        emcc.addFileArg(src_path);
    }

    for (compile_steps) |compile_step| {
        emcc.addArtifactArg(compile_step);
        for (compile_step.root_module.getGraph().modules) |module| {
            for (module.link_objects.items) |link_object| {
                switch (link_object) {
                    .other_step => |linked_compile_step| {
                        switch (linked_compile_step.kind) {
                            .lib => {
                                emcc.addArtifactArg(linked_compile_step);
                            },
                            else => {},
                        }
                    },
                    else => {},
                }
            }
        }
    }

    emcc.addArg("-o");
    const out_file = emcc.addOutputFileArg(options.out_file_name);

    if (options.use_preload_plugins) {
        emcc.addArg("--use-preload-plugins");
    }

    if (options.embed_paths) |embed_paths| {
        for (embed_paths) |path| {
            emcc.addArg("--embed-file");
            emcc.addFileArg(path.src_path);
        }
    }

    if (options.preload_paths) |preload_paths| {
        for (preload_paths) |path| {
            emcc.addArg("--preload-file");
            emcc.addFileArg(path.src_path);
        }
    }

    if (options.shell_file_path) |shell_file_path| {
        emcc.addArg("--shell-file");
        emcc.addFileArg(shell_file_path);
    }

    if (options.js_library_path) |js_library_path| {
        emcc.addArg("--js-library");
        emcc.addFileArg(js_library_path);
    }

    const install_step = b.addInstallDirectory(.{
        .source_dir = out_file.dirname(),
        .install_dir = options.install_dir,
        .install_subdir = "",
    });
    install_step.step.dependOn(&emcc.step);

    return &install_step.step;
}

pub fn emrunStep(
    b: *std.Build,
    html_path: std.Build.LazyPath,
    extra_args: []const []const u8,
) *std.Build.Step {
    var emrun = b.addRunFile(emrunPath(b));
    emrun.addArgs(extra_args);
    emrun.addFileArg(html_path);
    // emrun.addArg("--");

    return &emrun.step;
}

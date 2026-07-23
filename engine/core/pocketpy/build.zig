const std = @import("std");

/// All pocketpy C source files, relative to `engine/core/pocketpy/src/`.
const c_source_files = [_][]const u8{
    // common
    "common/algorithm.c",         "common/chunkedvector.c",   "common/dmath.c",
    "common/_generated.c",        "common/memorypool.c",      "common/name.c",
    "common/serialize.c",         "common/smallmap.c",        "common/socket.c",
    "common/sourcedata.c",        "common/sstream.c",         "common/str.c",
    "common/threads.c",           "common/vector.c",
    // compiler
             "compiler/compiler.c",
    "compiler/lexer.c",
    // debugger
              "debugger/core.c",          "debugger/dap.c",
    // interpreter
    "interpreter/ceval.c",        "interpreter/dll.c",        "interpreter/frame.c",
    "interpreter/generator.c",    "interpreter/heap.c",       "interpreter/line_profiler.c",
    "interpreter/objectpool.c",   "interpreter/py_compile.c", "interpreter/typeinfo.c",
    "interpreter/vm.c",           "interpreter/vmx.c",
    // bindings
           "bindings/py_array.c",
    "bindings/py_mappingproxy.c", "bindings/py_method.c",     "bindings/py_number.c",
    "bindings/py_object.c",       "bindings/py_property.c",   "bindings/py_range.c",
    "bindings/py_str.c",
    // modules
             "modules/array2d.c",        "modules/base64.c",
    "modules/builtins.c",         "modules/colorcvt.c",       "modules/conio.c",
    "modules/dis.c",              "modules/easing.c",         "modules/enum.c",
    "modules/gc.c",               "modules/importlib.c",      "modules/inspect.c",
    "modules/json.c",             "modules/lz4.c",            "modules/math.c",
    "modules/os.c",               "modules/pickle.c",         "modules/picoterm.c",
    "modules/pkpy.c",             "modules/random.c",         "modules/stdc.c",
    "modules/time.c",             "modules/traceback.c",      "modules/unicodedata.c",
    "modules/vmath.c",
    // objects
               "objects/bintree.c",        "objects/codeobject.c",
    "objects/codeobject_ser.c",   "objects/container.c",      "objects/namedict.c",
    "objects/object.c",
    // public
              "public/Bindings.c",        "public/CodeExecution.c",
    "public/DictSlots.c",         "public/FrameOps.c",        "public/GlobalSetup.c",
    "public/Inspection.c",        "public/ModuleSystem.c",    "public/PyDict.c",
    "public/PyException.c",       "public/PyList.c",          "public/PySlice.c",
    "public/PythonOps.c",         "public/PyTuple.c",         "public/StackOps.c",
    "public/TypeSystem.c",        "public/ValueCast.c",       "public/ValueCreation.c",
};

/// Result returned by `buildPocketpy`. The caller must:
/// - Import the Zig module:    `exe.root_module.addImport("pocketpy", result.module)`
/// - Link the C library:       `exe.root_module.linkLibrary(result.library)`
pub const PocketPyBuild = struct {
    module: *std.Build.Module,
    library: *std.Build.Step.Compile,
};

/// Build pocketpy as a static C library and create its Zig wrapper module.
pub fn buildPocketpy(
    b: *std.Build,
    target: std.Build.ResolvedTarget,
    optimize: std.builtin.OptimizeMode,
) !PocketPyBuild {
    // ---- translate-c: pocketpy.h → Zig type declarations ----
    const translate_c = b.addTranslateC(.{
        .root_source_file = b.path("engine/core/pocketpy/include/pocketpy.h"),
        .target = target,
        .optimize = optimize,
    });
    translate_c.addIncludePath(b.path("engine/core/pocketpy/include"));
    const c_mod = translate_c.createModule();

    // ---- C static library ----
    const lib = b.addLibrary(.{
        .name = "pocketpy",
        .linkage = .static,
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
        }),
    });

    var c_flags: std.ArrayList([]const u8) = .empty;
    try c_flags.append(b.allocator, "-std=c11");

    if (optimize != .Debug) {
        try c_flags.append(b.allocator, "-DNDEBUG");
    }
    if (target.result.os.tag == .windows) {
        try c_flags.append(b.allocator, "/utf-8");
        try c_flags.append(b.allocator, "/experimental:c11atomics");
    }

    lib.root_module.addIncludePath(b.path("engine/core/pocketpy/include"));
    lib.root_module.addCSourceFiles(.{
        .root = b.path("engine/core/pocketpy/src"),
        .files = &c_source_files,
        .flags = c_flags.items,
    });

    // ---- Zig wrapper module (imports the translate-c module internally) ----
    const mod = b.addModule("pocketpy", .{
        .root_source_file = b.path("engine/core/pocketpy/pocketpy.zig"),
        .target = target,
        .optimize = optimize,
        .imports = &.{
            .{ .name = "pocketpy_c", .module = c_mod },
        },
    });

    return PocketPyBuild{
        .module = mod,
        .library = lib,
    };
}

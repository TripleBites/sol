const std = @import("std");
const pocketpy_build = @import("engine/core/pocketpy/build.zig");

// pub fn build(b: *std.Build) void {
//     const target = b.standardTargetOptions(.{});
//     const optimize = b.standardOptimizeOption(.{});

//     // -------------------------------------------------------------------
//     // PocketPy  (C static library + Zig wrapper module)
//     // -------------------------------------------------------------------
//     const pk = pocketpy_build.buildPocketpy(b, target, optimize) catch @panic("pocketpy build failed");

//     // -------------------------------------------------------------------
//     // Sol engine executable
//     // -------------------------------------------------------------------
//     const exe = b.addExecutable(.{
//         .name = "sol",
//         .root_module = b.createModule(.{
//             .root_source_file = b.path("engine/platform/termux.zig"),
//             .target = target,
//             .optimize = optimize,
//             .imports = &.{
//                 .{ .name = "pocketpy", .module = pk.module },
//             },
//         }),
//     });

//     exe.root_module.linkLibrary(pk.library);

//     b.installArtifact(exe);

//     // -------------------------------------------------------------------
//     // Run step  –  `zig build run`
//     // -------------------------------------------------------------------
//     const run_cmd = b.addRunArtifact(exe);
//     run_cmd.step.dependOn(b.getInstallStep());
//     run_cmd.addPassthruArgs();

//     const run_step = b.step("run", "Run Sol engine");
//     run_step.dependOn(&run_cmd.step);

//     // -------------------------------------------------------------------
//     // Test step  –  `zig build test`
//     // -------------------------------------------------------------------
//     const exe_tests = b.addTest(.{
//         .root_module = b.createModule(.{
//             .root_source_file = b.path("engine/core/main.zig"),
//             .target = target,
//             .optimize = optimize,
//             .imports = &.{
//                 .{ .name = "pocketpy", .module = pk.module },
//             },
//         }),
//     });
//     exe_tests.root_module.linkLibrary(pk.library);

//     const run_exe_tests = b.addRunArtifact(exe_tests);
//     const test_step = b.step("test", "Run unit tests");
//     test_step.dependOn(&run_exe_tests.step);

//     // -------------------------------------------------------------------
//     // PocketPy unit tests  –  `zig build test-pk`
//     // -------------------------------------------------------------------
//     // We need to re-create the translate-c module for the isolated test.
//     const translate_c = b.addTranslateC(.{
//         .root_source_file = b.path("engine/core/pocketpy/include/pocketpy.h"),
//         .target = target,
//         .optimize = optimize,
//     });
//     translate_c.addIncludePath(b.path("engine/core/pocketpy/include"));
//     const pk_c_mod = translate_c.createModule();

//     const pk_tests = b.addTest(.{
//         .root_module = b.createModule(.{
//             .root_source_file = b.path("engine/core/pocketpy/pocketpy.zig"),
//             .target = target,
//             .optimize = optimize,
//             .imports = &.{
//                 .{ .name = "pocketpy_c", .module = pk_c_mod },
//             },
//         }),
//     });
//     pk_tests.root_module.linkLibrary(pk.library);

//     const run_pk_tests = b.addRunArtifact(pk_tests);
//     const test_pk_step = b.step("test-pk", "Run pocketpy unit tests");
//     test_pk_step.dependOn(&run_pk_tests.step);
// }

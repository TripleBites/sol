//! PocketPy bindings for the Sol Game Engine.
//!
//! PocketPy 2.1.8 is a pure-C11 Python interpreter. The C API is translated
//! at build time via `@import("pocketpy_c")` (generated from pocketpy.h).
//! This module wraps common operations in safe, idiomatic Zig functions.

const std = @import("std");
const c = @import("pocketpy_c");

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

/// Initialize pocketpy and the default VM. Must be called once before any
/// other pocketpy function.
pub fn initialize() void {
    c.py_initialize();
}

/// Finalize pocketpy and free all VMs. Irreversible – after this call no
/// pocketpy function may be used.
pub fn finalize() void {
    c.py_finalize();
}

// ---------------------------------------------------------------------------
// Execution helpers
// ---------------------------------------------------------------------------

/// Execute a Python source string in the main module.
/// Returns true on success. On error, prints the traceback and returns false.
pub fn exec(source: [:0]const u8) bool {
    const ok = c.py_exec(source, "<string>", c.EXEC_MODE, null);
    if (!ok) {
        c.py_printexc();
    }
    return ok;
}

/// Evaluate a Python expression in the main module.
/// Returns true on success (result in py_retval). On error, prints the
/// traceback and returns false.
pub fn eval(source: [:0]const u8) bool {
    const ok = c.py_eval(source, null);
    if (!ok) {
        c.py_printexc();
    }
    return ok;
}

// ---------------------------------------------------------------------------
// Reflection helpers
// ---------------------------------------------------------------------------

/// Return pocketpy version string (e.g. "2.1.8").
pub fn versionString() [:0]const u8 {
    return &c.PK_VERSION.*;
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------
// NOTE: py_finalize() is irreversible, so we call py_initialize() only once
// per test process. Tests that need the VM share a single lazy init.

var _initialized = false;
fn ensureInit() void {
    if (!_initialized) {
        initialize();
        _initialized = true;
    }
}

test "pocketpy initialize and simple exec" {
    ensureInit();

    try std.testing.expect(exec("x = 1 + 2"));
    try std.testing.expect(eval("x * 10"));
}

test "pocketpy version" {
    try std.testing.expect(std.mem.startsWith(u8, versionString(), "2."));
}

test "pocketpy hello world" {
    ensureInit();

    try std.testing.expect(exec(
        \\x = 10
        \\y = 20
        \\z = x + y
    ));
    try std.testing.expect(eval("z == 30"));

    try std.testing.expect(exec(
        \\greeting = "Hello"
        \\target  = "pocketpy"
    ));
    try std.testing.expect(eval("greeting + ' ' + target"));
}

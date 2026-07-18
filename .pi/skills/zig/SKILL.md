---
name: zig
description: Zig programming language (0.16/0.17) — build system, standard library patterns, testing, C interop, and idiomatic error handling. Use when writing, editing, debugging, or building Zig code.
---

# Zig Programming Skill

## Quick Reference

### Project Structure
```
project/
├── build.zig            # Build script
├── build.zig.zon        # Package manifest
└── src/
    └── main.zig         # Entry point with pub fn main(init: std.process.Init)
```

### Build Commands
```bash
zig build               # Default build (compile + install)
zig build run           # Build and run
zig build test          # Run tests
zig build --help        # See all options
zig build -Dtarget=x86_64-windows-gnu  # Cross compile
zig build -Doptimize=ReleaseSafe        # Optimized build
```

### Entry Point (Zig 0.17.0-dev)

The `main` function signature changed. It now takes `std.process.Init`:

```zig
const std = @import("std");
const Io = std.Io;

pub fn main(init: std.process.Init) !void {
    const arena = init.arena.allocator();
    const args = try init.minimal.args.toSlice(arena);

    // Buffered stdout via init.io
    var stdout_buf: [1024]u8 = undefined;
    var stdout_fw: Io.File.Writer = .init(.stdout(), init.io, &stdout_buf);
    const stdout = &stdout_fw.interface;

    try stdout.print("Hello, {s}!\n", .{"world"});
    try stdout.flush();

    // Unbuffered stderr (debug/log output)
    std.debug.print("Diagnostic message\n", .{});
}
```

### Standard Patterns

**Allocators:**
```zig
// Arena — lives for process lifetime
const arena = init.arena.allocator();

// GPA — general purpose (for tests or explicit management)
const gpa = std.testing.allocator;

// Create a child arena for short-lived work
var child = std.heap.ArenaAllocator.init(arena);
defer child.deinit();
const ca = child.allocator();
```

**Error handling:**
```zig
// try — propagate errors upward
const file = try std.fs.cwd().openFile("data.txt", .{});

// catch — handle inline
const n = std.fmt.parseInt(u32, str, 10) catch 0;

// if with error capture
if (doThing()) |val| { ... } else |err| { ... }
```

**Slices and lists:**
```zig
var list = std.ArrayList(u8).empty;
defer list.deinit(allocator);
try list.appendSlice(allocator, "hello");
// list.items is []u8
```

**Structs and enums:**
```zig
const Config = struct {
    width: u32,
    height: u32,
};
const Mode = enum { debug, release };
```

### C Interop (for pocketpy)
```zig
const c = @cImport({
    @cInclude("pocketpy.h");
});
// Use c.pkpy_vm_create() etc.
```

### Testing
```zig
test "name" {
    try std.testing.expectEqual(expected, actual);
}
```

### Key Differences Zig 0.16 → 0.17

| Feature | 0.16 | 0.17 |
|---------|------|------|
| `main` sig | `pub fn main() !void` | `pub fn main(init: std.process.Init) !void` |
| stdout | `std.io.getStdOut().writer()` | `Io.File.Writer.init(.stdout(), init.io, &buf)` |
| `build.zig` exe | `.root_source_file = ...` | `.root_module = b.createModule(...)` |
| `build.zig.zon` `.name` | `.name = "string"` | `.name = .enum_literal` |
| `build.zig.zon` | no fingerprint | `.fingerprint = 0x...` + `.minimum_zig_version` |

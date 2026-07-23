const std = @import("std");

// At startup:
// \x1b[?25l  -> hide cursor
// \x1b[2J    -> clear once initially

// At shutdown:
// \x1b[?25h  -> show cursor
// \x1b[0m    -> reset colors

pub const Color = struct {
    r: u8,
    g: u8,
    b: u8,
};

pub const Canvas = struct {
    width: usize,
    height: usize,
    pixels: []Color,
    allocator: std.mem.Allocator,

    pub fn init(allocator: std.mem.Allocator, width: usize, height: usize) !Canvas {
        const pixels = try allocator.alloc(Color, width * height);
        @memset(pixels, Color{ .r = 0, .g = 0, .b = 0 });
        return Canvas{
            .width = width,
            .height = height,
            .pixels = pixels,
            .allocator = allocator,
        };
    }

    pub fn deinit(self: *Canvas) void {
        self.allocator.free(self.pixels);
    }

    pub fn set(self: *Canvas, x: usize, y: usize, c: Color) void {
        if (x >= self.width or y >= self.height) return;
        self.pixels[y * self.width + x] = c;
    }

    pub fn get(self: *const Canvas, x: usize, y: usize) Color {
        if (x >= self.width or y >= self.height) return Color{ .r = 0, .g = 0, .b = 0 };
        return self.pixels[y * self.width + x];
    }
};

const UPPER_HALF_BLOCK = "▀"; // U+2580, 3 bytes in UTF-8

pub fn render(canvas: *const Canvas, writer: anytype) !void {
    var y: usize = 0;
    while (y < canvas.height) : (y += 2) {
        var x: usize = 0;
        while (x < canvas.width) : (x += 1) {
            const top = canvas.get(x, y);
            const has_bottom = (y + 1) < canvas.height;
            const bottom = if (has_bottom) canvas.get(x, y + 1) else top;

            // Set foreground (top pixel) + background (bottom pixel)
            try writer.print("\x1b[38;2;{d};{d};{d}m\x1b[48;2;{d};{d};{d}m{s}", .{
                top.r,            top.g,    top.b,
                bottom.r,         bottom.g, bottom.b,
                UPPER_HALF_BLOCK,
            });
        }
        try writer.print("\x1b[0m\n", .{}); // reset SGR, newline
    }
}

pub fn renderOptimized(canvas: *const Canvas, writer: anytype) !void {
    var y: usize = 0;
    while (y < canvas.height) : (y += 2) {
        var prev_fg: ?Color = null;
        var prev_bg: ?Color = null;

        var x: usize = 0;
        while (x < canvas.width) : (x += 1) {
            const top = canvas.get(x, y);
            const has_bottom = (y + 1) < canvas.height;
            const bottom = if (has_bottom) canvas.get(x, y + 1) else top;

            if (prev_fg == null or !colorEq(prev_fg.?, top)) {
                try writer.print("\x1b[38;2;{d};{d};{d}m", .{ top.r, top.g, top.b });
                prev_fg = top;
            }
            if (prev_bg == null or !colorEq(prev_bg.?, bottom)) {
                try writer.print("\x1b[48;2;{d};{d};{d}m", .{ bottom.r, bottom.g, bottom.b });
                prev_bg = bottom;
            }
            try writer.writeAll(UPPER_HALF_BLOCK);
        }
        try writer.print("\x1b[0m\n", .{});
    }
}

fn colorEq(a: Color, b: Color) bool {
    return a.r == b.r and a.g == b.g and a.b == b.b;
}

pub fn drawFrame(canvas: *const Canvas, allocator: std.mem.Allocator) !void {
    var buf = std.ArrayList(u8).init(allocator);
    defer buf.deinit();

    // Move cursor home instead of clearing (avoids flicker)
    try buf.writer().print("\x1b[H", .{});
    try renderOptimized(canvas, buf.writer());

    const stdout = std.io.getStdOut().writer();
    try stdout.writeAll(buf.items);
}

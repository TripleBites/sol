const std = @import("std");

// Framebuffer (RGBA) ──> [ 3-3-2 Bit Shift ] ──> Band Mask Buffer (256 x W)
//                                                          │
//                                                [ Truncate Trailing 0s ]
//                                                          │
//                                                   [ Sixel RLE ]
//                                                          │
// Single writeAll() Syscall <── Out Buffer <─────── [ Append Stream ]

pub const SixelRenderer = struct {
    width: usize,
    height: usize,
    stride: usize, // Pixels per row in source framebuffer

    // Pre-computed Sixel palette header bytes
    palette_header: [256 * 20 + 16]u8 = undefined,
    palette_header_len: usize = 0,

    // Scratchpad arrays for a single 6-pixel high band
    // 256 colors x max_width 6-bit masks
    masks: []u6,
    active_colors: std.StaticBitSet(256),
    last_nonzero_x: [256]usize,

    pub fn init(allocator: std.mem.Allocator, width: usize, height: usize, stride: usize) !SixelRenderer {
        var self = SixelRenderer{
            .width = width,
            .height = height,
            .stride = stride,
            .masks = try allocator.alloc(u6, 256 * width),
            .active_colors = std.StaticBitSet(256).initEmpty(),
            .last_nonzero_x = undefined,
        };

        self.initStaticPalette();
        return self;
    }

    pub fn deinit(self: *SixelRenderer, allocator: std.mem.Allocator) void {
        allocator.free(self.masks);
    }

    /// Pre-computes the 3-3-2 RGB palette header in Sixel format.
    /// Sixel RGB values are percentages (0..100).
    fn initStaticPalette(self: *SixelRenderer) void {
        var stream = std.io.fixedBufferStream(&self.palette_header);
        const writer = stream.writer();

        // DCS sequence: \x1bP7;1;4q -> 1:1 aspect ratio Sixel mode
        writer.writeAll("\x1bP7;1;4q") catch unreachable;

        var i: usize = 0;
        while (i < 256) : (i += 1) {
            const r_3bit: u8 = @intCast((i >> 5) & 0x07);
            const g_3bit: u8 = @intCast((i >> 2) & 0x07);
            const b_2bit: u8 = @intCast(i & 0x03);

            // Scale to Sixel 0..100 percentage range
            const r_pct = (r_3bit * 100) / 7;
            const g_pct = (g_3bit * 100) / 7;
            const b_pct = (b_2bit * 100) / 3;

            // Format: #<id>;2;<r_pct>;<g_pct>;<b_pct>
            writer.print("#{d};2;{d};{d};{d}", .{ i, r_pct, g_pct, b_pct }) catch unreachable;
        }

        self.palette_header_len = stream.getWritten().len;
    }

    /// Quantizes RGBA8888 or RGB24 to 3-3-2 palette index via 3 bitwise ops.
    inline fn quantizePixel(r: u8, g: u8, b: u8) u8 {
        return ((r & 0xE0)) | ((g & 0xE0) >> 3) | ((b & 0xC0) >> 6);
    }

    /// Renders `framebuffer` into `out_buf`.
    /// `framebuffer` must contain RGB24 or RGBA32 pixels (3 or 4 bytes per pixel).
    pub fn renderFrame(
        self: *SixelRenderer,
        framebuffer: []const u8,
        bytes_per_pixel: usize,
        out_buf: *std.ArrayListUnmanaged(u8),
    ) !void {
        out_buf.clearRetainingCapacity();

        // 1. Output pre-computed header + static palette
        try out_buf.appendSlice(undefined, self.palette_header[0..self.palette_header_len]);

        const num_bands = (self.height + 5) / 6;

        var band: usize = 0;
        while (band < num_bands) : (band += 1) {
            const y_start = band * 6;

            // Clear band scratchpad
            @memset(self.masks, 0);
            self.active_colors.setValueRange(.{ .start = 0, .end = 256 }, false);
            @memset(&self.last_nonzero_x, 0);

            // 2. Transpose 6 pixel rows into vertical sixel bitmasks
            var row_off: u3 = 0;
            while (row_off < 6) : (row_off += 1) {
                const y = y_start + row_off;
                if (y >= self.height) break;

                const bit_weight: u6 = @as(u6, 1) << row_off;
                const row_bytes = y * self.stride * bytes_per_pixel;

                var x: usize = 0;
                while (x < self.width) : (x += 1) {
                    const px_idx = row_bytes + (x * bytes_per_pixel);
                    const color = quantizePixel(
                        framebuffer[px_idx],
                        framebuffer[px_idx + 1],
                        framebuffer[px_idx + 2],
                    );

                    const mask_idx = (@as(usize, color) * self.width) + x;
                    self.masks[mask_idx] |= bit_weight;

                    self.active_colors.set(color);
                    self.last_nonzero_x[color] = x;
                }
            }

            // 3. Serialize active color planes in this band using RLE
            var color_iter = self.active_colors.iterator(.{});
            var is_first_color = true;

            while (color_iter.next()) |color| {
                const max_x = self.last_nonzero_x[color];
                const mask_offset = color * self.width;

                if (!is_first_color) {
                    // Graphics Carriage Return: reset cursor to left margin for next color layer
                    try out_buf.append(undefined, '$');
                }
                is_first_color = false;

                // Select current color index
                var num_buf: [16]u8 = undefined;
                const color_cmd = std.fmt.bufPrint(&num_buf, "#{d}", .{color}) catch unreachable;
                try out_buf.appendSlice(undefined, color_cmd);

                // RLE encode up to max_x (Truncate trailing zero sixels!)
                var x: usize = 0;
                while (x <= max_x) {
                    const raw_mask = self.masks[mask_offset + x];
                    const sixel_char: u8 = raw_mask + 63; // Map 0..63 to ASCII '?'..'~'

                    var run_len: usize = 1;
                    while (x + run_len <= max_x and self.masks[mask_offset + x + run_len] == raw_mask) {
                        run_len += 1;
                    }

                    if (run_len > 3) {
                        // Sixel RLE Syntax: !<count><char>
                        const rle_cmd = std.fmt.bufPrint(&num_buf, "!{d}{c}", .{ run_len, sixel_char }) catch unreachable;
                        try out_buf.appendSlice(undefined, rle_cmd);
                    } else {
                        var i: usize = 0;
                        while (i < run_len) : (i += 1) {
                            try out_buf.append(undefined, sixel_char);
                        }
                    }
                    x += run_len;
                }
            }

            // Graphics Newline: end 6-pixel band and advance cursor down
            try out_buf.append(undefined, '-');
        }

        // 4. String Terminator (ST) sequence to exit Sixel mode
        try out_buf.appendSlice(undefined, "\x1b\\");
    }
};

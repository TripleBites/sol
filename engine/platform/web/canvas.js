/*
 *  canvas.js — Emscripten JS library for the Sol Engine web platform.
 *
 *  Provides:
 *    - Canvas 2D pixel-blitting (ImageData)
 *    - Keyboard state tracking (key code → bool)
 *    - Mouse position + button tracking
 *
 *  Usage from Zig:
 *    extern fn canvas_init(width: i32, height: i32) void;
 *    extern fn canvas_put_pixels(ptr: [*]const u8, size: i32) void;
 *    extern fn key_is_down(key_code: [*:0]const u8) i32;
 *    extern fn mouse_get_x() i32;
 *    extern fn mouse_get_y() i32;
 *    extern fn mouse_button_down(btn: i32) i32;
 */

mergeInto(LibraryManager.library, {

    // ── Canvas 2D initialisation ─────────────────────────────────────────────
    canvas_init__deps: ['$UTF8ToString'],
    canvas_init: function(width, height) {
        var canvas = document.getElementById('canvas');
        if (!canvas) {
            console.error('Sol: no <canvas id="canvas"> found');
            return;
        }

        canvas.width = width;
        canvas.height = height;

        // Pixel-art friendly scaling
        canvas.style.imageRendering = 'pixelated';
        canvas.style.width  = Math.min(window.innerWidth,  width  * 3) + 'px';
        canvas.style.height = Math.min(window.innerHeight, height * 3) + 'px';

        Module._solCtx       = canvas.getContext('2d');
        Module._solImageData = Module._solCtx.createImageData(width, height);

        // ── Window resize handler ────────────────────────────────────────
        window.addEventListener('resize', function() {
            var w = Math.min(window.innerWidth,  width  * 3);
            var h = Math.min(window.innerHeight, height * 3);
            canvas.style.width  = w + 'px';
            canvas.style.height = h + 'px';
        });
    },

    // ── Blit the engine's pixel buffer to the canvas ─────────────────────────
    canvas_put_pixels: function(ptr, size) {
        var view = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, size);
        Module._solImageData.data.set(view);
        Module._solCtx.putImageData(Module._solImageData, 0, 0);
    },

    // ── Keyboard state ────────────────────────────────────────────────────────
    _solKeys: null,

    _solEnsureKeys: function() {
        if (Module._solKeys !== null) return;
        Module._solKeys = {};
        document.addEventListener('keydown', function(e) {
            Module._solKeys[e.code] = true;
            e.preventDefault();
        });
        document.addEventListener('keyup', function(e) {
            Module._solKeys[e.code] = false;
            e.preventDefault();
        });
    },

    key_is_down__deps: ['$UTF8ToString'],
    key_is_down: function(keyCodePtr) {
        Module._solEnsureKeys();
        var code = UTF8ToString(keyCodePtr);
        return Module._solKeys[code] ? 1 : 0;
    },

    // ── Mouse state ───────────────────────────────────────────────────────────
    _solMouseX: 0,
    _solMouseY: 0,
    _solMouseButtons: [false, false, false],

    _solEnsureMouse: function() {
        if (Module._solMouseInited) return;
        Module._solMouseInited = true;
        var canvas = document.getElementById('canvas');
        if (!canvas) return;

        canvas.addEventListener('mousemove', function(e) {
            var rect = canvas.getBoundingClientRect();
            var sx = canvas.width  / rect.width;
            var sy = canvas.height / rect.height;
            Module._solMouseX = Math.floor((e.clientX - rect.left) * sx);
            Module._solMouseY = Math.floor((e.clientY - rect.top)  * sy);
        });
        canvas.addEventListener('mousedown', function(e) {
            if (e.button < 3) Module._solMouseButtons[e.button] = true;
            e.preventDefault();
        });
        canvas.addEventListener('mouseup', function(e) {
            if (e.button < 3) Module._solMouseButtons[e.button] = false;
            e.preventDefault();
        });
    },

    mouse_get_x: function() { Module._solEnsureMouse(); return Module._solMouseX; },
    mouse_get_y: function() { Module._solEnsureMouse(); return Module._solMouseY; },

    mouse_button_down: function(btn) {
        Module._solEnsureMouse();
        if (btn < 0 || btn > 2) return 0;
        return Module._solMouseButtons[btn] ? 1 : 0;
    },
});

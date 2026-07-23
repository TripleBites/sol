
# Build and open in browser:
zig build -Dtarget=wasm32-emscripten emrun

# Or just build, then serve manually: 
zig build -Dtarget=wasm32-emscripten 
python3 -m http.server 8000 -d zig-out/web/
# Open http://localhost:8000/sol.html 
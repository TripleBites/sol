# Sol Game Engine
Zig based game engine that leverages pocketpy as the developers scripting language.

# Setup Instructions

# Directory Structure

### How to use

 ```bash
   # Development (debug)
   zig build -Dplatform=web
   zig-out/web/sol.html  # open in browser

   # Production (release)
   zig build -Dplatform=web
 -Doptimize=ReleaseSmall

   # Serve locally (on desktop)
   python3 -m http.server 8080 -d
 zig-out/web/
   # then open
 http://localhost:8080/sol.html

   # Native smoke test (verifies engine API
 works)
   zig build -Dplatform=native run
 ```

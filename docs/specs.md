# Sol Game Engine
Zig based game engine that leverages Micropython as the developers scripting language.

## Required Tools
- zig 0.16.0

## Allowed Programming Languages:
- zig: Core of the game engine. Anything that needs performance goes here.
- micropython: Used for game engine's scripting language and in engine utility.

## Target Platforms
- Windows
- Linux
- Android
- Android Termux
- Webassmbly

## Libraries
- Micropython: https://github.com/micropython/micropython/tree/master
- zig-gamedev: https://github.com/zig-gamedev
    1) zsdl (sdl3)
    2) zgpu (Dawn Cross Platform WebGPU)
    3) zgui (Dear Imgui, Test engine, ImPlot, ImGuizmo and imgui-node-editor)
    4) zjobs
    5) zflecs (flecs ECS)
    6) zmath
    7) zmesh
    8) znoise
    9) zstbi
    10) ztracy
  
## Directory Layout
```text
sol/
├── .vscode/                    # Setup for vscode engine development.
├── docs/
│   ├── agents/
│   ├── specs.md                # Sol game engine specification.
├── game/                       # Directory for developer assets and micropython code
│   ├── assets/                 # Game assets
│   └── code/                   # Micropython game logic code
├── src/                        # Main application source code
│   ├── core/            
│   │   └── main.zig            # Sol engine main loop and entry point              
│   ├── editor/   
│   │   └── editor.mpy          # Precompiled micropython game engine editor/
│   └── libs/        
│       └── micropython/        # Micropyton source code
│           ├── build.zig       # Micropyton source code zig builder
│           └── build.zig.zon   # Micropyton source code zig package manager if needed
├── .gitignore                  # Files to ignore in version control
└── README.md                   # Introduction to engine and getting started guide
```

## All of the 

## CLI Usage
Sol engine run from the cli will have:
```text
sol                         # Runs src/editor/editor.mpy
sol --help                  # Show cli usage
sol <directory path>        # Run the directory pointed at as a game directory containing all of the game assets and code. (sol ./game/)
sol <path to python file>   # Run just that micropython script in the engine. (sol ./game/code/game.py)
```

## Build Chain
Sol will be a stand alone engine executable. Micropython, zig-gamedev and its sub libraries will all be compiled into a single executable. And generate a single webassmbly wasm file for running the engine and games in the browser. All game assets and micropython scripts will be run by the engine binary in debug mode. release mode will package the entire engine, and precompile the micropython game scripts and package up the assets. So that the entire game can be run in a single executable.

## Engine Goals
Simple 2D and 3D engine running on Dawn WebGPU for full crossplatform and web support.
We will focus heavily on 2D at first. But 3D capabilities, must be first class in the future and allow for 2.5D game creation.

## Staring Goal
1) Get zig-gamedev and micropython building and callable in src/core/main.zig using our build.zig files.
2) Write test cases in src/core/main.zig that verify that all of our libraries are loading and building correctly.
3) Commit changes locally using git.
4) Get our main.py running a basic hello world print statement in game/code/game.py.
3) Commit changes locally using git.
6) Add engine logger with fun color support and UTF-8 characters with emoji support.
3) Commit changes locally using git.
8) Add engine tracing using ztracy.
3) Commit changes locally using git.
10) Open an zsdl SDL3 window in src/core/main.zig. Must use SDL3 and not SDl2.
11) Render a simple color gradient triangle to the window using zgpu.

## AI Agent Usage
All AI agent planning be located in docs/agents/ directory. But we do not want agents placing markdown and text files all over the repository. We want to keep it clean, simple, and readable, and easily iterable. 
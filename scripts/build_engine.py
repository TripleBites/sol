#!/usr/bin/env python3
"""Build the Sol C99 engine as a shared library for ctypes loading."""
import subprocess
import sys
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ENGINE_SRC = ROOT / "src" / "engine"
OUTPUT_DIR = ROOT / "src" / "sol"
SHADERS_DIR = ENGINE_SRC / "shaders"


def assemble_shaders():
    """Assemble SPIR-V shaders if source changed."""
    vertex_asm = SHADERS_DIR / "vertex.vert"
    fragment_asm = SHADERS_DIR / "fragment.frag"
    ui_vert_asm = SHADERS_DIR / "ui_vert.vert"
    ui_frag_asm = SHADERS_DIR / "ui_frag.frag"
    vertex_spv = SHADERS_DIR / "vertex.spv"
    fragment_spv = SHADERS_DIR / "fragment.spv"
    ui_vert_spv = SHADERS_DIR / "ui_vert.spv"
    ui_frag_spv = SHADERS_DIR / "ui_frag.spv"
    header_out = ENGINE_SRC / "shaders.h"

    need_rebuild = False
    for src, dst in [(vertex_asm, vertex_spv), (fragment_asm, fragment_spv),
                     (ui_vert_asm, ui_vert_spv), (ui_frag_asm, ui_frag_spv)]:
        if not dst.exists() or src.stat().st_mtime > dst.stat().st_mtime:
            need_rebuild = True
    if not header_out.exists():
        need_rebuild = True
    else:
        for _, dst in [(vertex_asm, vertex_spv), (fragment_asm, fragment_spv),
                       (ui_vert_asm, ui_vert_spv), (ui_frag_asm, ui_frag_spv)]:
            if dst.stat().st_mtime > header_out.stat().st_mtime:
                need_rebuild = True

    if need_rebuild:
        print("[build] Assembling SPIR-V shaders...")
        for src, dst in [(vertex_asm, vertex_spv), (fragment_asm, fragment_spv),
                         (ui_vert_asm, ui_vert_spv), (ui_frag_asm, ui_frag_spv)]:
            subprocess.run(["spirv-as", str(src), "-o", str(dst)], check=True)

        # Generate combined header
        with open(header_out, "w") as hf:
            hf.write("/* Auto-generated SPIR-V shader bytecode */\n")
            hf.write("#ifndef SOL_SHADERS_H\n#define SOL_SHADERS_H\n\n")
            hf.write("#include <stdint.h>\n\n")

            for spv_file, name in [(vertex_spv, "vertex_spv"), (fragment_spv, "fragment_spv"),
                                    (ui_vert_spv, "ui_vert_spv"), (ui_frag_spv, "ui_frag_spv")]:
                data = spv_file.read_bytes()
                hf.write(f"static const unsigned char {name}[] = {{\n  ")
                hf.write(", ".join(f"0x{b:02x}" for b in data))
                hf.write(f"\n}};\n")
                hf.write(f"static const unsigned int {name}_len = {len(data)};\n\n")

            hf.write("#endif /* SOL_SHADERS_H */\n")
        print(f"[build] Generated {header_out}")


def build_engine():
    """Compile the engine into a shared library."""
    assemble_shaders()

    system = sys.platform
    if system == "win32":
        libname = "sol.dll"
    elif system == "darwin":
        libname = "libsol.dylib"
    else:
        libname = "libsol.so"

    sources = [
        ENGINE_SRC / "engine.c",
        ENGINE_SRC / "platform" / "platform_sdl3.c",
        ENGINE_SRC / "platform" / "platform_vulkan.c",
        ENGINE_SRC / "platform" / "platform_headless.c",
        ENGINE_SRC / "ui" / "node.c",
        ENGINE_SRC / "ui" / "control.c",
        ENGINE_SRC / "ui" / "draw_list.c",
        ENGINE_SRC / "ui" / "scene_tree.c",
        ENGINE_SRC / "ui" / "color_rect.c",
        ENGINE_SRC / "ui" / "vbox_container.c",
    ]

    # Check if any source changed
    output = OUTPUT_DIR / libname
    if output.exists():
        out_mtime = output.stat().st_mtime
        rebuild = False
        for src in sources:
            if src.stat().st_mtime > out_mtime:
                rebuild = True
                break
        if not rebuild:
            print(f"[build] {output} is up to date.")
            return str(output)

    print("[build] Compiling Sol engine...")

    include_dirs = [
        str(ENGINE_SRC),
        "/usr/local/include",   # SDL3
    ]

    # Find SDL3 flags via pkg-config
    sdl3_cflags = []
    sdl3_libs = []
    try:
        cflags = subprocess.check_output(["pkg-config", "--cflags", "sdl3"], text=True).strip()
        sdl3_cflags = cflags.split()
        libs = subprocess.check_output(["pkg-config", "--libs", "sdl3"], text=True).strip()
        sdl3_libs = libs.split()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("[build] Warning: pkg-config sdl3 failed, using defaults")

    libs = sdl3_libs + ["-lvulkan", "-lm"]

    cc = os.environ.get("CC", "gcc")
    cflags = [cc, "-std=c99", "-O3", "-fPIC", "-shared"]
    cflags += sdl3_cflags
    for inc in include_dirs:
        cflags.append(f"-I{inc}")

    cmd = cflags + [str(s) for s in sources] + ["-o", str(output)] + libs

    print(f"[build] {' '.join(cmd)}")
    subprocess.run(cmd, check=True)
    print(f"[build] Built {output}")
    return str(output)


if __name__ == "__main__":
    build_engine()

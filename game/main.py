"""Sol Engine - Hello Triangle"""
import sol


def main():
    print("Initializing Sol Engine (SDL3 + Vulkan)...")

    if not sol.init("Sol Engine - Hello Triangle", 400, 400):
        print("ERROR: Failed to initialize engine")
        return 1

    w, h = sol.get_size()
    print(f"Framebuffer: {w}x{h}")

    print("Running... close the window to exit.")
    running = True
    frame = 0
    while running:
        running = sol.update()
        frame += 1
        if frame % 100 == 0:
            w, h = sol.get_size()
            print(f"Frame {frame} - {w}x{h}")

    sol.shutdown()
    print("Sol Engine closed cleanly.")
    return 0


if __name__ == "__main__":
    exit(main())

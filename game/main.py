import time
import numpy as np
import pandas as pd
import sol  # Our compiled C extension

def main():
    print("Initializing Python Application...")
    
    # Simple Pandas / NumPy sanity test
    df = pd.DataFrame({"x": np.arange(5), "y": np.arange(5) ** 2})
    print("NumPy/Pandas Test Matrix:\n", df)

    # Initialize C Engine powered by SDL3
    print("Launching C99 SDL3 Sol Engine...")
    sol.init("C99 Sol SDL3 Engine", 800, 600)

    # Simple frame loop (running 100 frames for demonstration)
    for _ in range(100):
        sol.update()
        time.sleep(0.016)  # ~60 FPS

    sol.shutdown()
    print("Sol Engine closed cleanly.")

if __name__ == "__main__":
    main()
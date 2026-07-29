

# Desktop Dev Workflow
uv pip install -e .  ---> Invokes setup.py ---> Uses Desktop GCC/Clang ---> engine.so (Linux Host)

# Buildozer Android Workflow
buildozer android debug
  ├── 1. Downloads NDK Toolchain (aarch64-linux-android-clang)
  ├── 2. Cross-compiles Python 3 runtime for Android
  ├── 3. Executes `python setup.py build_ext` using NDK env vars ($CC, $CFLAGS, $LDFLAGS)
  └── 4. Packages `engine.so`, Python code, NumPy, & Pandas into the final APK

# Develop and Run Commands

```shell
# 1. Ensure system SDL3 is installed (e.g., via package manager or built from source)
# 2. Create local venv and install dependencies
uv venv
source .venv/bin/activate
uv pip install numpy pandas setuptools

# 3. Build C extension in-place for local testing
python setup.py build_ext --inplace

# 4. Run application
python main.py
```

# Build Release for Android Using Buildozer
```shell
# Build APK and deploy directly to connected Android device via ADB
buildozer android debug deploy run

# View live logcat output filtered for Python/Engine stdout
buildozer android logcat | grep -E "python|SDL"
```

# Build Release for Desktop Using Nuitka
```shell
uv pip install nuitka
python setup.py build_ext --inplace

# Linux / Windows Standalone Binary Build
python -m nuitka \
    --standalone \
    --enable-plugin=numpy \
    --include-module=engine \
    main.py
```


# How the Unified Architecture Works Across Platforms
```text
               ┌──► Android APK  ───► Buildozer (p4a + NDK) ───► engine.so
               │
setup.py ──────┼──► Linux Exec.  ───► Nuitka ───► engine.so
(C99 + SDL3)   │
               └──► Windows EXE  ───► Nuitka ───► engine.pyd
```
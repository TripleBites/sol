[app]
title = Vulkan Engine
package.name = vulkanengine
package.domain = org.example
source.dir = .
source.include_exts = py,png,jpg,so

# Requirements
requirements = python3, numpy, pandas, hostpython3

# Target Android Architectures
android.archs = arm64-v8a, x86_64

# Vulkan requires Android NDK API level 24+
android.minapi = 24
android.ndk_api = 24
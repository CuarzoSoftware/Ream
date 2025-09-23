# Ream

<p align="left">
  <a href="https://github.com/CuarzoSoftware/Ream/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-LGPLv2.1-blue.svg" alt="Ream is released under the LGPLv2.1 license." />
  </a>
  <a href="https://github.com/CuarzoSoftware/Ream">
    <img src="https://img.shields.io/badge/version-0.1.0-brightgreen" alt="Current Ream version." />
  </a>
</p>

**Ream** is a C++ graphics library for Linux. It provides generic buffer allocation and rendering utilities, abstracting platform and graphics APIs while still allowing direct access to API-specific features when needed.

### Features

- RAII-based resource management
- Platform abstraction
- Graphics API abstraction
- Flexible buffer allocation
- Multi-threading with automatic or manual synchronization
- High-performance built-in painter
- Out-of-the-box Skia integration
- Hybrid GPU support

### Supported Platforms

- DRM
- Wayland
- Offscreen

### Supported Graphics APIs

- Raster
- OpenGL ES 2.0
- Vulkan (WIP)

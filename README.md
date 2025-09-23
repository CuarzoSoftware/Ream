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

### Usage Overview

1. Create an **RCore** instance.
2. Ream automatically selects an optimal primary device (when `nullptr` is passed to parameters expecting an **RDevice**).
3. Create an **RSurface** or **RSwapchain** as your render target, or wrap an existing **RImage** with an **RSurface**.
4. Configure the surface geometry (viewport, destination rect, transform, etc.).
5. Begin an **RPass** from the surface, specifying whether **RPaint** and/or **SkCanvas** will be used.
6. Render into the surface using **RPainter** and/or **SkCanvas**.
7. Manually destroy or let the pass go out of scope to flush commands and let Ream insert read/write fences.

# Ream

<p align="left">
  <a href="https://github.com/CuarzoSoftware/Ream/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-LGPLv2.1-blue.svg" alt="Ream is released under the LGPLv2.1 license." />
  </a>
  <a href="https://github.com/CuarzoSoftware/Ream">
    <img src="https://img.shields.io/badge/version-0.1.0-brightgreen" alt="Current Ream version." />
  </a>
</p>

**Ream** is a C++ graphics library for Linux. It provides generic buffer allocation and rendering utilities, abstracting the underlying platform and graphics API while still allowing direct access to API-specific features when needed.

It is developed primarily as the rendering foundation for Wayland compositors, but is useful for any Linux application that needs to allocate GPU buffers, share them across devices via DMA-buf, and draw into them with a portable API.

> Check the releases section for stable versions. The web documentation corresponds to the latest release.

## Links

- [📖 C++ API Documentation](https://cuarzosoftware.github.io/Ream/annotated.html)
- [🧩 Abstraction](https://cuarzosoftware.github.io/Ream/abstraction_page.html)
- [🕹️ Examples](https://cuarzosoftware.github.io/Ream/examples_page.html)
- [⚙️ Environment](https://cuarzosoftware.github.io/Ream/environment_page.html)
- [💬 Contact](https://cuarzosoftware.github.io/Ream/contact_page.html)

## ⭐ Features

- RAII-based resource management
- Platform abstraction
- Graphics API abstraction
- Flexible buffer allocation (native, GBM/DMA-buf, dumb buffers)
- Multi-threading with automatic or manual synchronization
- High-performance built-in painter
- Out-of-the-box [Skia](https://skia.org) integration
- Hybrid (multi-)GPU support with cross-device DMA-buf sharing
- Explicit synchronization via `sync_file` fences

## 🧩 Supported Platforms

- DRM
- Wayland
- Offscreen

## 💻 Supported Graphics APIs

- Raster
- OpenGL ES 2.0
- Vulkan (WIP)

## 🔩 Abstraction

Ream is designed to abstract away the underlying platform and graphics API (GAPI), allowing your code to remain platform- and API-independent by default.

To ensure portability, use the classes provided directly under the `CZ/Ream` directory. For example, to create an image, use `RImage::Make()`, which will internally return an appropriate implementation such as `RGLImage`, `RSImage`, or `RVKImage`, depending on the active GAPI.

Platform- and API-specific implementations reside in the `CZ/Ream/GL`, `CZ/Ream/RS`, `CZ/Ream/VK`, etc. subdirectories. In cases where you need access to API-specific functionality, most classes provide safe downcasting methods, e.g. `image->asGL()`.

The `RCore` class exposes information about the current platform and GAPI. It can be accessed globally via `RCore::Get()`.

See the [Abstraction](https://cuarzosoftware.github.io/Ream/abstraction_page.html) page for details.

## ⚙️ Environment Variables

* **CZ_REAM_GAPI**: Overrides the graphics API selection when `RCore` is initialized with `RGraphicsAPI::Auto`. Accepted values are `GL` (OpenGL), `VK` (Vulkan), and `RS` (Raster).

* **CZ_REAM_LOG_LEVEL**: Sets the minimum log level for runtime output. Available levels (from least to most verbose): 0: Silent, 1: Fatal, 2: Error, 3: Warning, 4: Info, 5: Debug, 6: Trace.

The full list, including backend-specific variables, is documented on the [Environment](https://cuarzosoftware.github.io/Ream/environment_page.html) page.

## 🛠️ Building

Ream uses the [Meson](https://mesonbuild.com) build system:

```bash
meson setup build
ninja -C build
sudo ninja -C build install
```

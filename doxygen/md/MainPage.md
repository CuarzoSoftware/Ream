# 🏠 Home {#MainPage}

**Ream** is a C++ graphics library for Linux. It provides generic buffer allocation and rendering utilities, abstracting the underlying platform and graphics API while still allowing direct access to API-specific features when needed.

It is developed primarily as the rendering foundation for Wayland compositors, but is useful for any Linux application that needs to allocate GPU buffers, share them across devices via DMA-buf, and draw into them with a portable API.

## ⭐ Features

- RAII-based resource management
- Platform abstraction (DRM, Wayland, Offscreen)
- Graphics API abstraction (Raster, OpenGL ES, Vulkan)
- Flexible buffer allocation (native, GBM/DMA-buf, dumb buffers)
- Multi-threading with automatic or manual synchronization
- High-performance built-in painter
- Out-of-the-box [Skia](https://skia.org) integration
- Hybrid (multi-)GPU support with cross-device DMA-buf sharing
- Explicit synchronization via `sync_file` fences

## 🧩 Supported Platforms

- **DRM** — direct KMS scanout
- **Wayland** — nested / client swapchains
- **Offscreen** — headless rendering

## 💻 Supported Graphics APIs

- **Raster** (`RS`) — CPU software rendering
- **OpenGL ES 2.0** (`GL`)
- **Vulkan** (`VK`)

## 🚀 Getting Started

Ream is designed so that most code can remain platform- and API-independent. Start from the
generic classes under the `CZ::` namespace — for example, initialize the library with
`CZ::RCore::Make()`, allocate an image with `CZ::RImage::Make()`, and render into it through a
`CZ::RPass`. See the @ref abstraction_page page for how the generic API maps onto concrete
backends, and the @ref examples_page page for a runnable Wayland swapchain example.

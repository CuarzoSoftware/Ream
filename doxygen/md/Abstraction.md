# 🧩 Abstraction {#abstraction_page}

Ream is designed to abstract away the underlying platform and graphics API (GAPI), allowing your
code to remain platform- and API-independent by default.

## Generic API

To ensure portability, use the classes provided directly under the `CZ/Ream` directory (the
`CZ::` namespace). For example, to create an image, use `CZ::RImage::Make()`, which internally
returns an appropriate implementation such as `RGLImage`, `RSImage`, or `RVKImage`, depending on
the active GAPI.

| Concept              | Generic class          | Purpose                                                       |
| :------------------- | :--------------------- | :------------------------------------------------------------ |
| Library entry point  | `CZ::RCore`            | Initialization, platform/GAPI info, device enumeration        |
| GPU device           | `CZ::RDevice`          | A rendering device (one per GPU); capabilities and formats    |
| Buffer / texture     | `CZ::RImage`           | Allocation, DMA-buf import/export, pixel upload/download       |
| Render target        | `CZ::RSurface`         | A drawable wrapping an `RImage`                                |
| Render pass          | `CZ::RPass`            | Scoped rendering into a surface (painter and/or Skia canvas)   |
| Built-in renderer    | `CZ::RPainter`         | High-performance native drawing (`drawColor`, `drawImage`, …)  |
| Synchronization      | `CZ::RSync`            | GPU/CPU fences, `sync_file` import/export                      |
| Presentation         | `CZ::RSwapchain`       | Platform swapchains (e.g. Wayland client)                      |

## API-specific access

Platform- and API-specific implementations live in the `CZ/Ream/GL`, `CZ/Ream/RS`,
`CZ/Ream/VK`, etc. subdirectories.

When you need access to API-specific functionality, most classes provide safe downcasting
methods, e.g. `image->asGL()`, `core->asVK()`. These return `nullptr` when the object does not
belong to that backend, so they double as a runtime backend check.

```cpp
if (auto *gl = image->asGL())
{
    // Use RGLImage-specific functionality (e.g. the underlying GL texture).
}
```

## Selecting a backend

The `CZ::RCore` class exposes information about the current platform and GAPI and can be accessed
globally via `CZ::RCore::Get()`.

The GAPI is chosen when `RCore` is created. Passing `RGraphicsAPI::Auto` lets Ream pick a backend
automatically, honoring the `CZ_REAM_GAPI` environment variable (see @ref environment_page).

## Interoperability

Because synchronization is expressed as `sync_file` fds and buffers as DMA-buf, Ream interoperates
cleanly with the wider Linux graphics stack: GBM, DRM/KMS framebuffers, DRM sync objects/timelines,
and other clients or compositors — regardless of which GAPI produced the buffer. This is what makes
hybrid-GPU (Prime) rendering possible: an image allocated on one GPU can be exported and sampled on
another.

# 🕹️ Examples {#examples_page}

Example programs live under `src/examples/` and are built by default (see the `build_examples`
Meson option).

## cz-ream-wl-swapchain

A minimal Wayland client that creates a `CZ::RWLSwapchain`, acquires a buffer each frame, draws an
animated scene into it using a Skia canvas obtained from a `CZ::RPass`, and presents it back to the
compositor. It works unchanged across the Raster, OpenGL, and Vulkan backends — switch with the
`CZ_REAM_GAPI` environment variable (see @ref environment_page):

```bash
# Run under each backend (requires a running Wayland compositor):
CZ_REAM_GAPI=GL ./cz-ream-wl-swapchain
CZ_REAM_GAPI=VK ./cz-ream-wl-swapchain
CZ_REAM_GAPI=RS ./cz-ream-wl-swapchain
```

The example demonstrates the core render loop: swapchain creation and resize, buffer-age tracking
for damage-aware rendering, wrapping the acquired image in an `CZ::RSurface`, and presenting.

# ⚙️ Environment Variables {#environment_page}

## General

* **CZ_REAM_GAPI**: Overrides the graphics API selection when `CZ::RCore` is initialized with
  `RGraphicsAPI::Auto`. Accepted values are `GL` (OpenGL), `VK` (Vulkan), and `RS` (Raster).

## Debugging

* **CZ_REAM_LOG_LEVEL**: Sets the minimum log level for runtime output. Accepts an integer from
  least to most verbose:
  `0` Silent, `1` Fatal, `2` Error, `3` Warning, `4` Info, `5` Debug, `6` Trace.

* **CZ_REAM_EGL_LOG_LEVEL**: Same scale as `CZ_REAM_LOG_LEVEL`, but for the OpenGL/EGL backend.

* **CZ_REAM_VK_LOG_LEVEL**: Same scale as `CZ_REAM_LOG_LEVEL`, but for the Vulkan backend. When
  raised, the Vulkan validation layers (if installed) are enabled and their messages are routed
  through the logger.

## Vulkan Backend

* **CZ_REAM_VK_ALLOW_CPU**: Set to `1` to allow selecting CPU/software Vulkan devices (e.g.
  llvmpipe). Disabled by default, since software devices are usually undesirable for a compositor;
  useful for testing and headless setups.

* **CZ_REAM_VK_SYNC_SUBMIT**: Set to `1` to force synchronous (blocking) command submission in the
  Vulkan painter instead of the default asynchronous, fence-tracked submission. Intended for
  debugging and A/B performance comparisons.

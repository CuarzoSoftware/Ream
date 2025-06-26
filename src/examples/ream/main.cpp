#include <WL/RWLPlatformHandle.h>
#include <RLog.h>
#include <RCore.h>

#include <wayland-client.h>

using namespace CZ;

int main()
{
    setenv("REAM_DEBUG", "4", 0);
    setenv("REAM_EGL_DEBUG", "4", 0);

    wl_display *wlDisplay { wl_display_connect(NULL) };

    if (!wlDisplay)
    {
        RFatal(RLINE, "Failed to create wl_display.");
        return EXIT_FAILURE;
    }

    RCore::Options options {};
    options.graphicsAPI = RGraphicsAPI::GL;
    options.platformHandle = RWLPlatformHandle::Make(wlDisplay);
    auto core { RCore::Make(options) };

    return 0;
}

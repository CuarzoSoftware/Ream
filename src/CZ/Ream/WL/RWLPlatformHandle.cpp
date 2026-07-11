#include <CZ/Ream/WL/RWLPlatformHandle.h>
#include <CZ/Ream/RLog.h>
#include <wayland-client.h>
#include "linux-dmabuf-v1-client-protocol.h"
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <sys/sysmacros.h>

using namespace CZ;

namespace
{
// Minimal linux-dmabuf-v1 client query: reads the compositor's advertised main_device from the
// default feedback. Runs on a dedicated event queue with a display proxy wrapper so it never
// dispatches events belonging to the application's default queue.
struct QueryState
{
    zwp_linux_dmabuf_v1 *dmabuf { nullptr };
    zwp_linux_dmabuf_feedback_v1 *feedback { nullptr };
    dev_t mainDevice { 0 };
};

void fbMainDevice(void *data, zwp_linux_dmabuf_feedback_v1 *, wl_array *device) noexcept
{
    auto *s { static_cast<QueryState*>(data) };
    if (device->size == sizeof(dev_t))
        std::memcpy(&s->mainDevice, device->data, sizeof(dev_t));
}

void fbFormatTable(void *, zwp_linux_dmabuf_feedback_v1 *, int32_t fd, uint32_t) noexcept
{
    if (fd >= 0) close(fd);
}

void fbNop0(void *, zwp_linux_dmabuf_feedback_v1 *) noexcept {}
void fbNopArr(void *, zwp_linux_dmabuf_feedback_v1 *, wl_array *) noexcept {}
void fbNopFlags(void *, zwp_linux_dmabuf_feedback_v1 *, uint32_t) noexcept {}

const zwp_linux_dmabuf_feedback_v1_listener kFbListener {
    .done                  = fbNop0,
    .format_table          = fbFormatTable,
    .main_device           = fbMainDevice,
    .tranche_done          = fbNop0,
    .tranche_target_device = fbNopArr,
    .tranche_formats       = fbNopArr,
    .tranche_flags         = fbNopFlags,
};

void regGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version) noexcept
{
    auto *s { static_cast<QueryState*>(data) };
    if (!s->dmabuf && version >= 4 && std::strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0)
        s->dmabuf = static_cast<zwp_linux_dmabuf_v1*>(
            wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, 4));
}

void regGlobalRemove(void *, wl_registry *, uint32_t) noexcept {}

const wl_registry_listener kRegListener { .global = regGlobal, .global_remove = regGlobalRemove };

// Returns the compositor's dmabuf main_device, or 0 if unavailable.
dev_t QueryMainDevice(wl_display *display) noexcept
{
    if (!display)
        return 0;

    wl_event_queue *queue { wl_display_create_queue(display) };
    if (!queue)
        return 0;

    QueryState st {};
    wl_registry *registry { nullptr };

    // Wrap the display so the registry (and everything bound from it) dispatches to our own queue,
    // never stealing events from the application's default queue.
    auto *wrapped { static_cast<wl_display*>(wl_proxy_create_wrapper(display)) };
    if (wrapped)
    {
        wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapped), queue);
        registry = wl_display_get_registry(wrapped);
        wl_proxy_wrapper_destroy(reinterpret_cast<wl_proxy*>(wrapped));
    }

    if (registry)
    {
        wl_registry_add_listener(registry, &kRegListener, &st);

        // Receive globals, then bind + read the default feedback's main_device.
        if (wl_display_roundtrip_queue(display, queue) >= 0 && st.dmabuf)
        {
            st.feedback = zwp_linux_dmabuf_v1_get_default_feedback(st.dmabuf);
            if (st.feedback)
            {
                zwp_linux_dmabuf_feedback_v1_add_listener(st.feedback, &kFbListener, &st);
                wl_display_roundtrip_queue(display, queue);
            }
        }
    }

    if (st.feedback) zwp_linux_dmabuf_feedback_v1_destroy(st.feedback);
    if (st.dmabuf)   zwp_linux_dmabuf_v1_destroy(st.dmabuf);
    if (registry)    wl_registry_destroy(registry);
    wl_event_queue_destroy(queue);

    return st.mainDevice;
}
} // namespace

std::shared_ptr<RWLPlatformHandle> RWLPlatformHandle::Make(wl_display *wlDisplay, CZOwn ownership) noexcept
{
    if (!wlDisplay)
    {
        RLog(CZFatal, CZLN, "Invalid wl_display handle");
        return nullptr;
    }

    auto handle { std::shared_ptr<RWLPlatformHandle>(new RWLPlatformHandle(wlDisplay, ownership)) };
    handle->m_mainDevice = QueryMainDevice(wlDisplay);

    if (handle->m_mainDevice != 0)
        RLog(CZDebug, CZLN, "Compositor main device (dmabuf feedback): {}:{}",
             major(handle->m_mainDevice), minor(handle->m_mainDevice));

    return handle;
}

RWLPlatformHandle::~RWLPlatformHandle() noexcept
{
    if (m_wlDisplay && m_own == CZOwn::Own)
        wl_display_disconnect(m_wlDisplay);
}

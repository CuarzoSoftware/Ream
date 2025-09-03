#include <CZ/Ream/RS/RRSSwapchainWL.h>
#include <CZ/Ream/RS/RRSImage.h>
#include <CZ/Ream/RS/RRSCore.h>
#include <CZ/Ream/WL/RWLFormat.h>
#include <CZ/Ream/RLog.h>
#include <CZ/Utils/CZVectorUtils.h>

using namespace CZ;

RRSSwapchainWL::~RRSSwapchainWL() noexcept {}

std::optional<const RSwapchainImage> RRSSwapchainWL::acquire() noexcept
{
    if (m_acquired)
        return {};

    m_acquired = true;

    // Search for a released buffer

    std::shared_ptr<RRSSwapchainWL::Buffer> buffer;

    if (!m_releasedIdx.empty())
    {
        UInt32 bestAge { UINT32_MAX };

        for (auto it = m_releasedIdx.begin(); it != m_releasedIdx.end(); it++)
        {
            auto buff { m_buffers[*it] };
            UInt32 age { m_frame - buff->ssImage.frame };

            if (age < bestAge)
            {
                bestAge = age;
                buffer = buff;
            }
        }

        if (bestAge == 0)
            bestAge = 1;

        m_releasedIdx.erase(buffer->ssImage.index);
        buffer->ssImage.age = bestAge;
        buffer->ssImage.frame = m_frame++;
        buffer->resize(m_size);
    }

    // Or create a new one...

    if (!buffer)
    {
        buffer = Buffer::Make(this, m_size, 0, m_frame, m_buffers.size());
        m_buffers.emplace_back(buffer);
        m_frame++;
    }

    buffer->released = false;

    freeForgottenBuffers();

    return buffer->ssImage;
}

bool RRSSwapchainWL::present(const RSwapchainImage &image, SkRegion *damage) noexcept
{
    m_acquired = false;
    wl_surface_attach(m_surface, m_buffers[image.index]->buffer, 0, 0);

    if (!damage || wl_surface_get_version(m_surface) < 3)
        wl_surface_damage(m_surface, 0, 0, INT32_MAX, INT32_MAX);
    else
    {
        SkRegion::Iterator it { *damage };

        while (!it.done())
        {
            wl_surface_damage_buffer(m_surface, it.rect().x(), it.rect().y(), it.rect().width(), it.rect().height());
            it.next();
        }
    }

    wl_surface_commit(m_surface);
    return true;
}

bool RRSSwapchainWL::resize(SkISize size) noexcept
{
    if (size.isEmpty())
        return false;

    m_size = size;
    return true;
}

std::shared_ptr<RRSSwapchainWL> RRSSwapchainWL::Make(wl_surface *surface, SkISize size) noexcept
{
    auto core { RCore::Get()->asRS() };

    if (!core->m_shm)
    {
        RLog(CZError, CZLN, "Failed to create swapchain (missing wl_shm global)");
        return {};
    }

    auto swapchain { std::shared_ptr<RRSSwapchainWL>(new RRSSwapchainWL(core, core->mainDevice(), surface, size)) };
    return swapchain;
}

RRSSwapchainWL::RRSSwapchainWL(std::shared_ptr<RRSCore> core, RRSDevice *device, wl_surface *surface, SkISize size) noexcept :
    RWLSwapchain(size, surface),
    m_core(core), m_device(device)
{}

void RRSSwapchainWL::freeForgottenBuffers() noexcept
{
    if (m_buffers.size() <= 3)
        return;

    for (size_t i = 0; i < m_buffers.size();)
    {
        m_releasedIdx.clear();

        if (m_buffers[i]->released && m_frame - m_buffers[i]->ssImage.frame > 3)
        {
            m_buffers[i] = m_buffers.back();
            m_buffers[i]->ssImage.index = i;
            m_buffers.pop_back();

            if (i < m_buffers.size() && m_buffers[i]->released)
                m_releasedIdx.emplace(i);
        }
        else
        {
            m_buffers[i]->ssImage.index = i;

            if (m_buffers[i]->released)
                m_releasedIdx.emplace(i);

            i++;
        }
    }
}

std::shared_ptr<RRSSwapchainWL::Buffer> RRSSwapchainWL::Buffer::Make(RRSSwapchainWL *swapchain, SkISize size, UInt32 age, UInt32 frame, UInt32 index) noexcept
{
    auto image { RRSImage::Make(size, { DRM_FORMAT_ARGB8888, { DRM_FORMAT_MOD_LINEAR } } ) };

    if (!image)
        return {};

    RSwapchainImage ssImage {};
    ssImage.image = image;
    ssImage.age = age;
    ssImage.frame = frame;
    ssImage.index = index;

    auto *pool { wl_shm_create_pool(swapchain->m_core->m_shm, image->shm()->fd(), image->shm()->size()) };
    auto *buff { wl_shm_pool_create_buffer(pool, 0, size.width(), size.height(), image->stride(), RWLFormat::FromDRM(image->formatInfo().format)) };

    return std::shared_ptr<Buffer>(new Buffer(swapchain, ssImage, pool, buff));
}

void RRSSwapchainWL::Buffer::onRelease(void *data, wl_buffer *) noexcept
{
    auto *buffer { (Buffer*)data };
    buffer->released = true;

    auto *ss = buffer->swapchain.get();

    if (!ss) return;

    ss->m_releasedIdx.emplace(buffer->ssImage.index);
}

RRSSwapchainWL::Buffer::Buffer(RRSSwapchainWL *swapchain, const RSwapchainImage &ssImage, wl_shm_pool *pool, wl_buffer *buffer) noexcept :
    swapchain(swapchain),
    ssImage(ssImage),
    pool(pool),
    buffer(buffer)
{
    static const wl_buffer_listener listener{
        .release = onRelease
    };

    wl_buffer_add_listener(buffer, &listener, this);
}

void RRSSwapchainWL::Buffer::resize(SkISize size) noexcept
{
    if (ssImage.image->size() != size && !size.isEmpty())
    {
        wl_buffer_destroy(buffer);

        if (ssImage.image->asRS()->resize(size) == 1)
            wl_shm_pool_resize(pool, ssImage.image->asRS()->shm()->size());

        buffer = wl_shm_pool_create_buffer(
            pool, 0,
            size.width(), size.height(),
            ssImage.image->asRS()->stride(),
            RWLFormat::FromDRM(ssImage.image->asRS()->formatInfo().format));

        static const wl_buffer_listener listener{
            .release = onRelease
        };

        wl_buffer_add_listener(buffer, &listener, this);
    }
}

RRSSwapchainWL::Buffer::~Buffer() noexcept
{
    wl_buffer_destroy(buffer);
    wl_shm_pool_destroy(pool);
}

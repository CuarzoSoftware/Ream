#include <CZ/Ream/RLog.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Utils/CZVectorUtils.h>

#include <mutex>

using namespace CZ;

static UInt32 managerCount { 0 };
static UInt32 threadCount { 0 };
static std::recursive_mutex mutex;

struct CZ::RGLThreadDataManager
{
    struct DeviceData
    {
        EGLContext context { EGL_NO_CONTEXT };
    };

    std::thread::id mainThreadId;
    RPlatform platform;
    std::shared_ptr<RGLCore> glCore;
    std::unordered_map<RGLDevice*, DeviceData> devicesData;
    std::unordered_map<RGLDevice*, std::vector<RGLContextData*>> devicesGarbage;

    static RGLThreadDataManager &Get(bool create = true) noexcept
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        thread_local RGLThreadDataManager threadData;

        if (create && !threadData.glCore)
        {
            RDebug("+ (%d) RGLThreadDataManager.", ++threadCount);
            assert(RCore::Get());
            threadData.glCore = RCore::Get()->asGL();
            assert(threadData.glCore);
            threadData.glCore->m_threadDataManagers[std::this_thread::get_id()] = &threadData;
            threadData.platform = threadData.glCore->platform();
            threadData.mainThreadId = threadData.glCore->m_mainThread;
        }

        return threadData;
    };

    void clearGarbage() noexcept
    {
        while (!devicesGarbage.empty())
        {
            auto it { devicesGarbage.begin() };

            while (!it->second.empty())
            {
                auto current { RGLMakeCurrent::FromDevice(it->first, false) };
                delete it->second.back();
                it->second.pop_back();
            }

            devicesGarbage.erase(it);
        }
    }

    // Free all data
    void releaseData() noexcept
    {
        for (auto *contextDataManager : glCore->m_contextDataManagers)
            contextDataManager->freeCurrentThreadData();

        clearGarbage();

        // Destroy all contexts (there is no point in saving the prev context since this is called when the thread is destroyed)
        while (!devicesData.empty())
        {
            auto it { devicesData.begin() };
            eglMakeCurrent(it->first->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroyContext(it->first->eglDisplay(), it->second.context);
            devicesData.erase(it);
        }

        glCore->m_threadDataManagers.erase(std::this_thread::get_id());
        glCore.reset();
        RDebug("- (%d) RGLThreadDataManager.", --threadCount);
    }

    ~RGLThreadDataManager() noexcept
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);

        if (!glCore)
            return;

        releaseData();
    }

    DeviceData *getDeviceData(RGLDevice *device) noexcept
    {
        assert(device);
        auto &data { devicesData[device] };

        if (data.context != EGL_NO_CONTEXT)
            return &data;

        if (std::this_thread::get_id() == mainThreadId)
        {
            data.context = device->m_eglContext;
        }
        else
        {
            auto current { RGLMakeCurrent(device->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) };

            size_t atti { 0 };
            EGLint attribs[5];
            attribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
            attribs[atti++] = 2;

            if (platform == RPlatform::DRM && device->eglDisplayExtensions().IMG_context_priority)
            {
                attribs[atti++] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
                attribs[atti++] = EGL_CONTEXT_PRIORITY_HIGH_IMG;
            }

            attribs[atti++] = EGL_NONE;
            data.context = eglCreateContext(device->eglDisplay(), EGL_NO_CONFIG_KHR, device->m_eglContext, attribs);
        }

        return &data;
    }
};

bool RGLCore::init() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    RGLThreadDataManager::Get(true);
    if (initClientEGLExtensions() &&
        initDevices())
    {
        return true;
    }

    RGLThreadDataManager::Get(false).glCore.reset();
    return false;
}

void RGLCore::clearGarbage() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    RGLThreadDataManager::Get(false).clearGarbage();
}

void RGLCore::freeThread() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    RGLThreadDataManager::Get(false).releaseData();
}

EGLContext RGLDevice::eglContext() const noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);
    return RGLThreadDataManager::Get(false).getDeviceData((RGLDevice*)this)->context;
}

std::shared_ptr<RGLContextDataManager> RGLContextDataManager::Make(AllocFunc func) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    auto core { RCore::Get() };

    if (!core)
    {
        RError(CZLN, "Missing RCore.");
        return  {};
    }

    auto glCore { core->asGL() };

    if (!core)
    {
        RError(CZLN, "The current core must be an RGLCore.");
        return  {};
    }

    if (!func)
    {
        RError(CZLN, "Invalid allocator function.");
        return  {};
    }

    std::shared_ptr<RGLContextDataManager> manager { new RGLContextDataManager(func) };
    glCore->m_contextDataManagers.emplace_back(manager.get());
    RDebug("+ (%u) RGLContextDataManager.", ++managerCount);
    return manager;
}

RGLContextData *RGLContextDataManager::getData(RGLDevice *device) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
    {
        RError(CZLN, "Invalid device.");
        return nullptr;
    }

    auto &threadData { m_data[std::this_thread::get_id()] };
    auto it { threadData.find(device) };

    if (it != threadData.end())
        return it->second;

    auto *data { m_allocFunc(device) };

    if (!data)
        return nullptr;

    threadData[device] = data;
    return data;
}

void RGLContextDataManager::freeData(RGLDevice *device) noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (!device)
    {
        RError(CZLN, "Invalid device.");
        return;
    }

    if (!RCore::Get())
        return;

    auto core { RCore::Get()->asGL() };

    auto self { m_self.lock() };

    for (auto &threadMap : m_data)
    {
        auto it { threadMap.second.find(device) };

        // No data stored for this thread/device
        if (it == threadMap.second.end())
            continue;

        auto *data { it->second };

        // Delete data from the current thread right away
        if (threadMap.first == std::this_thread::get_id())
        {
            auto current { RGLMakeCurrent::FromDevice(device, false) };
            delete data;
        }
        // Mark it as garbage in other threads
        else
        {
            auto threadManagerIt { core->m_threadDataManagers.find(std::this_thread::get_id()) };
            assert(threadManagerIt != core->m_threadDataManagers.end());
            auto &deviceGarbage { threadManagerIt->second->devicesGarbage[device] };
            deviceGarbage.emplace_back(data);
        }

        threadMap.second.erase(it);
    }
}

RGLContextDataManager::RGLContextDataManager(const AllocFunc &func) noexcept :
    m_allocFunc(func)
{}

RGLContextDataManager::~RGLContextDataManager() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    if (RCore::Get())
    {
        auto core { RCore::Get()->asGL() };

        while (!m_data.empty())
        {
            auto it1 { m_data.begin() };

            while (!it1->second.empty())
            {
                auto it2 { it1->second.begin() };
                auto *data { it2->second };

                // Delete data from the current thread right away
                if (it1->first == std::this_thread::get_id())
                {
                    auto current { RGLMakeCurrent::FromDevice(it2->first, false) };
                    delete data;
                }
                // Mark it as garbage in other threads
                else
                {
                    auto threadManagerIt { core->m_threadDataManagers.find(std::this_thread::get_id()) };
                    assert(threadManagerIt != core->m_threadDataManagers.end());
                    auto &deviceGarbage { threadManagerIt->second->devicesGarbage[it2->first] };
                    deviceGarbage.emplace_back(data);
                }

                it1->second.erase(it2);
            }

            m_data.erase(it1);
        }

        CZVectorUtils::RemoveOneUnordered(core->m_contextDataManagers, this);
    }

    RDebug("- (%u) RGLContextDataManager.", --managerCount);
}

void RGLContextDataManager::freeCurrentThreadData() noexcept
{
    std::lock_guard<std::recursive_mutex> lock(mutex);

    auto it { m_data.find(std::this_thread::get_id()) };

    if (it == m_data.end())
        return;

    while (!it->second.empty())
    {
        auto begin { it->second.begin() };
        auto current { RGLMakeCurrent::FromDevice(begin->first, false) };
        delete begin->second;
        it->second.erase(begin);
    }
}

static UInt32 contextDataCount { 0 };

RGLContextData::RGLContextData() noexcept
{
    contextDataCount++;
    RDebug("+ (%d) RGLContextData.", contextDataCount);
}

RGLContextData::~RGLContextData() noexcept
{
    contextDataCount--;
    RDebug("- (%d) RGLContextData.", contextDataCount);
}

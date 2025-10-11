#include <CZ/Ream/RLog.h>
#include <CZ/Ream/RLockGuard.h>
#include <CZ/Ream/GL/RGLContext.h>
#include <CZ/Ream/GL/RGLCore.h>
#include <CZ/Ream/GL/RGLDevice.h>
#include <CZ/Ream/GL/RGLMakeCurrent.h>
#include <CZ/Ream/SK/RSKContext.h>
#include <CZ/Core/Utils/CZVectorUtils.h>

#include <CZ/skia/gpu/ganesh/gl/GrGLAssembleInterface.h>
#include <CZ/skia/gpu/ganesh/gl/GrGLDirectContext.h>

#include <mutex>
#include <unordered_set>

using namespace CZ;

static pthread_t mainThreadId { 0 };
static RPlatform platformType;

static auto skInterface = GrGLMakeAssembledInterface(nullptr, (GrGLGetProc)*[](void *, const char *p) -> void * {
    return (void *)eglGetProcAddress(p);
});

struct Threads
{
    size_t internal { 0 };

    void addContextDataManager(std::shared_ptr<RGLCore> core, RGLContextDataManager *mng) noexcept
    {
        if (core)
            glCore = core;
        else
            internal++;

        contextDataManagers.emplace(mng);
    }

    void removeContextDataManager(RGLContextDataManager *mng) noexcept
    {
        contextDataManagers.erase(mng);

        if (contextDataManagers.size() <= internal)
            glCore.reset();
    }

    std::shared_ptr<RGLCore> glCore;
    std::unordered_map<pthread_t, RGLThreadDataManager*> threadDataManagers;
    std::unordered_set<RGLContextDataManager*> contextDataManagers;
    std::unordered_set<pthread_t> deadThreads;
    std::unordered_map<pthread_t, std::shared_ptr<RGLThreadDataManager>> threads;
};

static Threads threads {};

struct DeadThreadCleaner
{
    DeadThreadCleaner() noexcept
    {
        threads.deadThreads.erase(pthread_self());
        threads.threads[pthread_self()] = std::make_shared<RGLThreadDataManager>();
    }

    ~DeadThreadCleaner() noexcept
    {
        threads.threads.erase(pthread_self());
    }
};

static std::shared_ptr<RGLThreadDataManager> GetData() noexcept
{
    RLockGuard lock {};
    thread_local DeadThreadCleaner tc {};

    auto it { threads.threads.find(pthread_self()) };

    if (it != threads.threads.end())
        return it->second;

    return {};
}

/* A thread_local struct to manage context-specific resources
 * See Get() */
struct CZ::RGLThreadDataManager
{
    /* Device data for this thread except for the main thread */
    struct DeviceData
    {
        EGLContext context { EGL_NO_CONTEXT };
        sk_sp<GrDirectContext> skContext;
    };

    /* Data of each device for this thread */
    std::unordered_map<RGLDevice*, DeviceData> devicesData;

    /* User data requested to be destroyed from another thread.
     * The destruction occurs when clearGarbage() is called from this thread
     * or this thread dies */
    std::unordered_map<RGLDevice*, std::vector<RGLContextData*>> devicesGarbage;

    RGLThreadDataManager() noexcept
    {
        threads.threadDataManagers.emplace(pthread_self(), this);
    }

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
        RLockGuard lock {};

        for (auto *contextDataManager : threads.contextDataManagers)
            contextDataManager->freeCurrentThreadData();

        clearGarbage();

        // Destroy all contexts (there is no point in saving the prev context since this is called when the thread is destroyed)
        while (!devicesData.empty())
        {
            auto it { devicesData.begin() };
            it->second.skContext.reset();

            if (threads.threadDataManagers.size() != 1)
            {
                eglMakeCurrent(it->first->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                eglDestroyContext(it->first->eglDisplay(), it->second.context);
                it->first->log(CZTrace, "GL context destroyed for thread {}", pthread_self());
            }

            devicesData.erase(it);
        }

        threads.threadDataManagers.erase(pthread_self());

        /* The main thread was not destroyed last */
        auto core { RCore::Get() };
        if (core && !threads.threadDataManagers.empty() && mainThreadId == pthread_self())
        {
            auto nextAliveThread { *threads.threadDataManagers.begin() };
            mainThreadId = nextAliveThread.first;
            RLog(CZTrace, "The main GL thread has changed to: {}", mainThreadId);
            for (auto &deviceData : nextAliveThread.second->devicesData)
            {
                eglMakeCurrent(deviceData.first->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                deviceData.first->m_eglContext = deviceData.second.context;
                deviceData.first->m_skContext = deviceData.second.skContext;
            }
        }
    }

    ~RGLThreadDataManager() noexcept
    {
        releaseData();
        threads.deadThreads.emplace(pthread_self());
        assert(devicesData.empty());
        assert(devicesGarbage.empty());
    }

    DeviceData *getDeviceData(RGLDevice *device) noexcept
    {
        assert(device);
        auto &data { devicesData[device] };

        if (data.context != EGL_NO_CONTEXT)
            return &data;

        /* The main context is created by the RGLDevice from the main thread */
        if (pthread_self() == mainThreadId)
        {
            data.context = device->m_eglContext;
            data.skContext = device->m_skContext;
        }
        else
        {
            auto current { RGLMakeCurrent(device->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) };

            size_t atti { 0 };
            EGLint attribs[5];
            attribs[atti++] = EGL_CONTEXT_CLIENT_VERSION;
            attribs[atti++] = 2;

            if (platformType == RPlatform::DRM && device->eglDisplayExtensions().IMG_context_priority)
            {
                attribs[atti++] = EGL_CONTEXT_PRIORITY_LEVEL_IMG;
                attribs[atti++] = EGL_CONTEXT_PRIORITY_HIGH_IMG;
            }

            attribs[atti++] = EGL_NONE;
            data.context = eglCreateContext(device->eglDisplay(), EGL_NO_CONFIG_KHR, device->m_eglContext, attribs);

            if (data.context == EGL_NO_CONTEXT)
                device->log(CZError, CZLN, "Failed to create shared GL context for thread {}", pthread_self());
            else
            {
                eglMakeCurrent(device->eglDisplay(), EGL_NO_SURFACE, EGL_NO_SURFACE, data.context);
                device->log(CZTrace, "Shared GL context created for thread {}", pthread_self());
                data.skContext = GrDirectContexts::MakeGL(skInterface, CZ::GetSKContextOptions());

                if (!data.skContext)
                    device->log(CZError, CZLN, "Failed to create GL GrDirectContext for thread {}", pthread_self());
            }
        }

        return &data;
    }
};

bool RGLCore::init() noexcept
{
    RLockGuard lock {};
    threads.internal = 0;
    mainThreadId = pthread_self();
    platformType = platform();

    GetData();

    if (initClientEGLExtensions() && initDevices())
        return true;

    return false;
}

void RGLCore::clearGarbage() noexcept
{
    RLockGuard lock {};
    if (auto t { GetData() })
        t->clearGarbage();
}

void RGLCore::freeThread() noexcept
{
    RLockGuard lock {};
    if (auto t { GetData() })
        t->releaseData();
}

EGLContext RGLDevice::eglContext() const noexcept
{
    RLockGuard lock {};

    if (pthread_self() == mainThreadId)
        return m_eglContext;

    if (auto t { GetData() })
        return t->getDeviceData((RGLDevice*)this)->context;

    return EGL_NO_CONTEXT;
}

sk_sp<GrDirectContext> RGLDevice::skContext() const noexcept
{
    RLockGuard lock {};

    if (pthread_self() == mainThreadId)
        return m_skContext;

    if (auto t { GetData() })
        return t->getDeviceData((RGLDevice*)this)->skContext;

    return nullptr;
}

void RGLDevice::wait() noexcept
{
    auto current { RGLMakeCurrent::FromDevice(this, false) };
    glFinish();
}

std::shared_ptr<RGLContextDataManager> RGLContextDataManager::Make(AllocFunc func) noexcept
{
    RLockGuard lock {};

    auto core { RCore::Get() };

    if (!core)
    {
        RLog(CZError, CZLN, "Missing RCore");
        return  {};
    }

    auto glCore { core->asGL() };

    if (!core)
    {
        RLog(CZError, CZLN, "Not an RGLCore");
        return  {};
    }

    if (!func)
    {
        RLog(CZError, CZLN, "Invalid allocator function");
        return  {};
    }

    std::shared_ptr<RGLContextDataManager> manager { new RGLContextDataManager(func) };
    threads.addContextDataManager(glCore, manager.get());
    return manager;
}

std::shared_ptr<RGLContextDataManager> RGLContextDataManager::MakeInternal(AllocFunc func) noexcept
{
    RLockGuard lock {};
    std::shared_ptr<RGLContextDataManager> manager { new RGLContextDataManager(func) };
    threads.addContextDataManager(nullptr, manager.get());
    return manager;
}

RGLContextData *RGLContextDataManager::getData(RGLDevice *device) noexcept
{
    RLockGuard lock {};

    if (!device)
    {
        RLog(CZError, CZLN, "Invalid device");
        return nullptr;
    }

    auto &threadData { m_data[pthread_self()] };
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
    RLockGuard lock {};

    if (!device)
    {
        RLog(CZError, CZLN, "Invalid device");
        return;
    }

    auto core { RCore::Get()};

    if (!core)
        return;

    auto self { m_self.lock() };

    for (auto &threadMap : m_data)
    {
        auto it { threadMap.second.find(device) };

        // No data stored for this thread/device
        if (it == threadMap.second.end())
            continue;

        auto *data { it->second };

        // Delete data from the current thread right away
        if (threadMap.first == pthread_self())
        {
            auto current { RGLMakeCurrent::FromDevice(device, false) };
            delete data;
        }
        // Mark it as garbage in other threads
        else
        {
            auto threadManagerIt { threads.threadDataManagers.find(threadMap.first) };
            assert (threadManagerIt != threads.threadDataManagers.end());

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
    RLockGuard lock {};

    auto core { RCore::Get() };

    if (core)
    {
        auto core { RCore::Get()->asGL() };

        while (!m_data.empty())
        {
            auto it1 { m_data.begin() }; // <thread, <device, data>>

            while (!it1->second.empty())
            {
                auto it2 { it1->second.begin() }; // <device, data>
                auto *data { it2->second };

                // Delete data from the current thread right away
                if (it1->first == pthread_self())
                {
                    auto current { RGLMakeCurrent::FromDevice(it2->first, false) };
                    delete data;
                }
                // Mark it as garbage in other threads
                else
                {
                    auto threadManagerIt { threads.threadDataManagers.find(it1->first) };
                    assert (threadManagerIt != threads.threadDataManagers.end());

                    auto &deviceGarbage { threadManagerIt->second->devicesGarbage[it2->first] };
                    deviceGarbage.emplace_back(data);
                }

                it1->second.erase(it2);
            }

            m_data.erase(it1);
        }

        threads.removeContextDataManager(this);
    }
}

void RGLContextDataManager::freeCurrentThreadData() noexcept
{
    RLockGuard lock {};

    auto it { m_data.find(pthread_self()) };

    if (it == m_data.end())
        return;

    while (!it->second.empty())
    {
        auto begin { it->second.begin() };
        auto current { RGLMakeCurrent::FromDevice(begin->first, false) };
        delete begin->second;
        it->second.erase(begin);
    }

    m_data.erase(it);
}

static UInt32 contextDataCount { 0 };

RGLContextData::RGLContextData() noexcept
{
    contextDataCount++;
}

RGLContextData::~RGLContextData() noexcept
{
    contextDataCount--;

    if (contextDataCount == 0)
        RLog(CZTrace, "RGLContextData count reached: 0");
}

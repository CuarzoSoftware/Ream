#include <CZ/Ream/RLockGuard.h>
#include <mutex>

using namespace CZ;

static std::mutex Mutex;
thread_local bool DidThreadLock { false };

bool RLockGuard::Lock() noexcept
{
    if (DidThreadLock)
        return false;

    DidThreadLock = true;
    Mutex.lock();
    return true;
}

bool RLockGuard::Unlock() noexcept
{
    if (!DidThreadLock)
        return false;

    DidThreadLock = false;
    Mutex.unlock();
    return true;
}

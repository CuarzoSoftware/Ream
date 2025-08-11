#ifndef CZ_RLOCKGUARD_H
#define CZ_RLOCKGUARD_H

namespace CZ
{
    struct RLockGuard
    {
        // Creates a scoped lock
        RLockGuard() noexcept : didLock(RLockGuard::Lock()) {}

        ~RLockGuard() noexcept
        {
            if (didLock)
                RLockGuard::Unlock();
        }

        // true if locked, false if already locked
        static bool Lock() noexcept;

        // true if unlocked, false if already unlocked
        static bool Unlock() noexcept;
    private:
        bool didLock;
    };
}
#endif // CZ_RLOCKGUARD_H

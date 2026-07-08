#ifndef CZ_RLOCKGUARD_H
#define CZ_RLOCKGUARD_H

namespace CZ
{
    /**
     * @brief RAII guard that serializes access to Ream's global lock.
     *
     * The lock is recursive per-thread: acquiring it while the current thread already
     * holds it is a no-op, and only the outermost guard releases it on destruction.
     *
     * @note Meant for internal use.
     */
    struct RLockGuard
    {
        /**
         * @brief Acquires the global lock for the duration of this scope.
         */
        RLockGuard() noexcept : didLock(RLockGuard::Lock()) {}

        /**
         * @brief Releases the lock if this guard acquired it.
         */
        ~RLockGuard() noexcept
        {
            if (didLock)
                RLockGuard::Unlock();
        }

        /**
         * @brief Locks the global mutex for the calling thread.
         *
         * @return `true` if the lock was acquired, `false` if the current thread already held it.
         */
        static bool Lock() noexcept;

        /**
         * @brief Unlocks the global mutex previously acquired by the calling thread.
         *
         * @return `true` if the lock was released, `false` if the current thread did not hold it.
         */
        static bool Unlock() noexcept;
    private:
        bool didLock;
    };
}
#endif // CZ_RLOCKGUARD_H

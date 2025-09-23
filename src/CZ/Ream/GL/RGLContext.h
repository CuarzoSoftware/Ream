#ifndef RGLCONTEXT_H
#define RGLCONTEXT_H

#include <CZ/Core/CZWeak.h>
#include <CZ/Ream/RObject.h>
#include <thread>
#include <memory>

/**
 * @brief Context-specific user data
 *
 * This class should only be instantiated through the allocator function provided to an RGLContextDataManager.
 *
 * Creating it elsewhere will result in resource leaks.
 */
class CZ::RGLContextData : public RObject
{
public:
    RGLContextData() noexcept;

    /* Populate it with your own OpenGL resources */

protected:
    friend struct RGLThreadDataManager;
    friend class RGLContextDataManager;
    ~RGLContextData() noexcept;
};

/**
 * @brief OpenGL context data manager.
 *
 * In Ream, each device has a dedicated OpenGL context for every thread that uses it.
 * Although these contexts are created as shared, they do not always share all OpenGL resources—
 * such as framebuffers, shader programs, and others, making it difficult for users to allocate
 * and manage their own graphics resources.
 *
 * This class enables users to provide a custom allocator function that creates an instance of a subclass
 * of RGLContextData, which can be populated with any required data members. The instance is automatically created
 * if one does not already exist for the current device/context combination when getData() is called.
 *
 * At allocation time, the EGL context for the corresponding device and thread is made current.
 * The same applies when the destructor of the associated RGLContextData is invoked.
 *
 * Each allocated RGLContextData instance is automatically destroyed under the following conditions:
 * - When its thread terminates
 * - When this manager is destroyed
 * - When you manually call freeData()
 */
class CZ::RGLContextDataManager : public RObject
{
public:
    /**
     * @brief Allocator function.
     *
     * This function is used to create instances of a subclass of RGLContextData associated
     * with a specific device/thread.
     *
     * @param device Pointer to the RGLDevice for which context-specific data is requested.
     * @return A pointer to a new subclass of RGLContextData containing the necessary data members.
     *         Returning `nullptr` is valid, in such cases, getData() will return `nullptr` for the
     *         corresponding thread/device pair.
     */
    using AllocFunc = std::function<RGLContextData *(RGLDevice *device)>;

    /**
     * @brief Creates an RGLContextDataManager.
     *
     * @param func Allocator function.
     *
     * @return A shared pointer to the created manager instance, or `nullptr` if the allocator is invalid.
     */
    static std::shared_ptr<RGLContextDataManager> Make(AllocFunc func) noexcept;

    /**
     * @brief Retrieves or creates context-specific data for the given device on the current thread.
     *
     * If no data exists for the current device/context pair, a new instance is allocated using the
     * registered allocator function. The associated EGL context is bound during allocation.
     *
     * @param device Pointer to the target device.
     * @return Pointer to the user data, or `nullptr` if the allocator functuin returned `nullptr`.
     */
    RGLContextData *getData(RGLDevice *device) noexcept;

    /**
     * @brief Frees all RGLContextData instances associated with the specified device across all threads.
     *
     * This method releases the context-specific data allocated for the given device in every thread
     * where it was used.
     *
     * @note Resources allocated on the current thread are destroyed immediately. For other threads,
     *       cleanup is deferred until either the threads are terminated or RGLCore::clearGarbage() is
     *       explicitly invoked from those threads.
     *
     * @param device Pointer to the target RGLDevice whose associated context data will be released.
     */
    void freeData(RGLDevice *device) noexcept;

    ~RGLContextDataManager() noexcept;
private:
    static std::shared_ptr<RGLContextDataManager> MakeInternal(AllocFunc func) noexcept;
    friend class RGLCore;
    friend class RGLDevice;
    friend struct RGLThreadDataManager;
    RGLContextDataManager(const AllocFunc &func) noexcept;
    void freeCurrentThreadData() noexcept;
    // Stored data for each [thread, device]
    std::unordered_map<pthread_t, std::unordered_map<RGLDevice*, RGLContextData*>> m_data;
    AllocFunc m_allocFunc;
    std::weak_ptr<RGLContextDataManager> m_self;
};

#endif // RGLCONTEXT_H

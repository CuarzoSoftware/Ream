#ifndef CZ_RVKEXTENSIONS_H
#define CZ_RVKEXTENSIONS_H

#include <vulkan/vulkan_core.h>

namespace CZ
{
    /**
     * @brief Instance-level function pointers resolved via vkGetInstanceProcAddr.
     */
    struct RVKInstanceProcs
    {
        PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT;
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT;
    };

    /**
     * @brief Device-level function pointers resolved via vkGetDeviceProcAddr.
     *
     * Entry points come from extensions that may or may not be present; unavailable
     * ones stay nullptr and their corresponding RVKDeviceExtensions flag stays false.
     */
    struct RVKDeviceProcs
    {
        // VK_KHR_external_memory_fd
        PFN_vkGetMemoryFdKHR getMemoryFdKHR;
        PFN_vkGetMemoryFdPropertiesKHR getMemoryFdPropertiesKHR;

        // VK_EXT_image_drm_format_modifier
        PFN_vkGetImageDrmFormatModifierPropertiesEXT getImageDrmFormatModifierPropertiesEXT;

        // VK_KHR_external_semaphore_fd
        PFN_vkImportSemaphoreFdKHR importSemaphoreFdKHR;
        PFN_vkGetSemaphoreFdKHR getSemaphoreFdKHR;

        // VK_KHR_timeline_semaphore (core in 1.2)
        PFN_vkGetSemaphoreCounterValueKHR getSemaphoreCounterValueKHR;
        PFN_vkWaitSemaphoresKHR waitSemaphoresKHR;
        PFN_vkSignalSemaphoreKHR signalSemaphoreKHR;

        // VK_KHR_synchronization2 (core in 1.3)
        PFN_vkQueueSubmit2KHR queueSubmit2KHR;
        PFN_vkCmdPipelineBarrier2KHR cmdPipelineBarrier2KHR;

        // VK_KHR_dynamic_rendering (core in 1.3)
        PFN_vkCmdBeginRenderingKHR cmdBeginRenderingKHR;
        PFN_vkCmdEndRenderingKHR cmdEndRenderingKHR;
    };

    /**
     * @brief Availability flags for the optional device extensions/features Ream uses.
     */
    struct RVKDeviceExtensions
    {
        bool KHR_swapchain;
        bool KHR_external_memory_fd;
        bool EXT_external_memory_dma_buf;
        bool EXT_image_drm_format_modifier;
        bool EXT_queue_family_foreign;
        bool KHR_image_format_list;
        bool KHR_external_semaphore_fd;
        bool KHR_timeline_semaphore;
        bool KHR_synchronization2;
        bool KHR_dynamic_rendering;
        bool EXT_physical_device_drm;
    };
}

#endif // CZ_RVKEXTENSIONS_H

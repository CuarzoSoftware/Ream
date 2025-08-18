#ifndef CZ_RVKEXTENSIONS_H
#define CZ_RVKEXTENSIONS_H

#include <vulkan/vulkan_core.h>

namespace CZ
{
    struct RVKInstanceProcs
    {
        PFN_vkCreateDebugUtilsMessengerEXT createDebugUtilsMessengerEXT;
        PFN_vkDestroyDebugUtilsMessengerEXT destroyDebugUtilsMessengerEXT;
    };
}

#endif // CZ_RVKEXTENSIONS_H

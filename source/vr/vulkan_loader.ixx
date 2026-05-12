module;
#include <windows.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

export module VR.VulkanLoader;
import VR.Log;

// Globals — function pointers
export PFN_vkGetInstanceProcAddr           vkGetInstanceProcAddr  = nullptr;
export PFN_vkCreateInstance                 vkCreateInstance       = nullptr;
export PFN_vkDestroyInstance                vkDestroyInstance      = nullptr;
export PFN_vkEnumeratePhysicalDevices       vkEnumeratePhysicalDevices = nullptr;
export PFN_vkGetPhysicalDeviceQueueFamilyProperties vkGetPhysicalDeviceQueueFamilyProperties = nullptr;
export PFN_vkCreateDevice                   vkCreateDevice         = nullptr;
export PFN_vkDestroyDevice                  vkDestroyDevice        = nullptr;
export PFN_vkGetDeviceQueue                 vkGetDeviceQueue       = nullptr;
export PFN_vkGetDeviceProcAddr              vkGetDeviceProcAddr    = nullptr;
export PFN_vkCreateCommandPool             vkCreateCommandPool            = nullptr;
export PFN_vkDestroyCommandPool            vkDestroyCommandPool           = nullptr;
export PFN_vkAllocateCommandBuffers        vkAllocateCommandBuffers       = nullptr;
export PFN_vkFreeCommandBuffers            vkFreeCommandBuffers           = nullptr;
export PFN_vkBeginCommandBuffer            vkBeginCommandBuffer           = nullptr;
export PFN_vkEndCommandBuffer              vkEndCommandBuffer             = nullptr;
export PFN_vkResetCommandBuffer            vkResetCommandBuffer           = nullptr;
export PFN_vkQueueSubmit                   vkQueueSubmit                  = nullptr;
export PFN_vkQueueWaitIdle                 vkQueueWaitIdle                = nullptr;
export PFN_vkCmdClearColorImage            vkCmdClearColorImage           = nullptr;
export PFN_vkCmdPipelineBarrier            vkCmdPipelineBarrier           = nullptr;

export PFN_vkCreateFence                 vkCreateFence                = nullptr;
export PFN_vkDestroyFence            vkDestroyFence           = nullptr;
export PFN_vkWaitForFences            vkWaitForFences           = nullptr;

namespace {
    HMODULE g_vulkanModule = nullptr;
}

export bool LoadVulkan() {
    g_vulkanModule = LoadLibraryW(L"vulkan-1.dll");
    if (!g_vulkanModule) {
        LogError("Failed to load vulkan-1.dll");
        return false;
    }
    
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)
        GetProcAddress(g_vulkanModule, "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) {
        LogError("Failed to get vkGetInstanceProcAddr");
        return false;
    }
    
    // Loader-level functions (no instance needed)
    #define LOAD(name) name = (PFN_##name) vkGetInstanceProcAddr(VK_NULL_HANDLE, #name)
    LOAD(vkCreateInstance);
    #undef LOAD
    
    LogInfo("Vulkan loader initialized");
    return true;
}

// Call after creating VkInstance to resolve instance-level functions
export void LoadVulkanInstanceFunctions(VkInstance instance) {
    #define LOAD(name) name = (PFN_##name) vkGetInstanceProcAddr(instance, #name)
    LOAD(vkDestroyInstance);
    LOAD(vkEnumeratePhysicalDevices);
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD(vkCreateDevice);
    LOAD(vkDestroyDevice);
    LOAD(vkGetDeviceQueue);
	LOAD(vkGetDeviceProcAddr);
	LOAD(vkCreateCommandPool);
	LOAD(vkDestroyCommandPool);
	LOAD(vkAllocateCommandBuffers);
	LOAD(vkFreeCommandBuffers);
	LOAD(vkBeginCommandBuffer);
	LOAD(vkEndCommandBuffer);
	LOAD(vkResetCommandBuffer);
	LOAD(vkQueueSubmit);
	LOAD(vkQueueWaitIdle);
	LOAD(vkCmdClearColorImage);
	LOAD(vkCmdPipelineBarrier);
	LOAD(vkCreateFence);
	LOAD(vkDestroyFence);
	LOAD(vkWaitForFences);
#undef LOAD
}

// Call after creating VkDevice to resolve device-level functions
//export void LoadVulkanDeviceFunctions(VkDevice device) {
//    #define LOAD(name) name = (PFN_##name) vkGetDeviceProcAddr(device, #name)
//    LOAD(vkDestroyDevice);
//    LOAD(vkGetDeviceQueue);
//    #undef LOAD
//}
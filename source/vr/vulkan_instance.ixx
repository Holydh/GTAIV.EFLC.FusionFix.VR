module;
#define VK_NO_PROTOTYPES
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#define VK_NO_PROTOTYPES
#include <openxr/openxr_platform.h>
#include <vector>

export module VR.VulkanInstance;
import VR.Log;
import VR.VulkanLoader;

export struct OurVulkanContext {
    VkInstance       instance         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice   = VK_NULL_HANDLE;
    VkDevice         device           = VK_NULL_HANDLE;
    VkQueue          queue            = VK_NULL_HANDLE;
    uint32_t         queueFamilyIndex = 0;
};

export bool CreateOurVulkan(XrInstance xrInstance, XrSystemId xrSystemId, 
                            OurVulkanContext& out);
export void DestroyOurVulkan(OurVulkanContext& ctx);

bool CreateOurVulkan(XrInstance xrInstance, XrSystemId xrSystemId, OurVulkanContext& out) {
    // 1. Required: query graphics requirements
    PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetReqs = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirements2KHR",
        (PFN_xrVoidFunction*)&pfnGetReqs);
    XrGraphicsRequirementsVulkanKHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    pfnGetReqs(xrInstance, xrSystemId, &reqs);
    
    // 2. Create VkInstance via OpenXR
    PFN_xrCreateVulkanInstanceKHR pfnCreateVkInst = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanInstanceKHR",
        (PFN_xrVoidFunction*)&pfnCreateVkInst);
    
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "GTAIV-VR";
    appInfo.apiVersion = VK_API_VERSION_1_1;
    
    VkInstanceCreateInfo vkInstCi{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    vkInstCi.pApplicationInfo = &appInfo;
    
    XrVulkanInstanceCreateInfoKHR xrVkInstCi{XR_TYPE_VULKAN_INSTANCE_CREATE_INFO_KHR};
    xrVkInstCi.systemId = xrSystemId;
    xrVkInstCi.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrVkInstCi.vulkanCreateInfo = &vkInstCi;
    
    VkResult vkResult;
    XrResult r = pfnCreateVkInst(xrInstance, &xrVkInstCi, &out.instance, &vkResult);
    if (XR_FAILED(r) || vkResult != VK_SUCCESS) {
        LogError("xrCreateVulkanInstanceKHR failed: xr=%d vk=%d", r, vkResult);
        return false;
    }
    LogInfo("Our VkInstance: %p", (void*)out.instance);
    
    LoadVulkanInstanceFunctions(out.instance);

    // 3. Get physical device
    PFN_xrGetVulkanGraphicsDevice2KHR pfnGetDev = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDevice2KHR",
        (PFN_xrVoidFunction*)&pfnGetDev);
    
    XrVulkanGraphicsDeviceGetInfoKHR devGetInfo{XR_TYPE_VULKAN_GRAPHICS_DEVICE_GET_INFO_KHR};
    devGetInfo.systemId = xrSystemId;
    devGetInfo.vulkanInstance = out.instance;
    r = pfnGetDev(xrInstance, &devGetInfo, &out.physicalDevice);
    if (XR_FAILED(r)) { LogError("GetVulkanGraphicsDevice2 failed"); return false; }
    LogInfo("Our VkPhysicalDevice: %p", (void*)out.physicalDevice);
    
    // 4. Find queue family supporting graphics
    uint32_t queueFamCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(out.physicalDevice, &queueFamCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFams(queueFamCount);
    vkGetPhysicalDeviceQueueFamilyProperties(out.physicalDevice, &queueFamCount, queueFams.data());
    
    out.queueFamilyIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueFamCount; ++i) {
        if (queueFams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            out.queueFamilyIndex = i;
            break;
        }
    }
    if (out.queueFamilyIndex == UINT32_MAX) {
        LogError("No graphics queue family"); return false;
    }
    
    // 5. Create VkDevice via OpenXR
    PFN_xrCreateVulkanDeviceKHR pfnCreateVkDev = nullptr;
    xrGetInstanceProcAddr(xrInstance, "xrCreateVulkanDeviceKHR",
        (PFN_xrVoidFunction*)&pfnCreateVkDev);
    
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCi.queueFamilyIndex = out.queueFamilyIndex;
    queueCi.queueCount = 1;
    queueCi.pQueuePriorities = &queuePriority;
    
    VkDeviceCreateInfo vkDevCi{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    vkDevCi.queueCreateInfoCount = 1;
    vkDevCi.pQueueCreateInfos = &queueCi;
    
    XrVulkanDeviceCreateInfoKHR xrVkDevCi{XR_TYPE_VULKAN_DEVICE_CREATE_INFO_KHR};
    xrVkDevCi.systemId = xrSystemId;
    xrVkDevCi.pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
    xrVkDevCi.vulkanPhysicalDevice = out.physicalDevice;
    xrVkDevCi.vulkanCreateInfo = &vkDevCi;
    
    r = pfnCreateVkDev(xrInstance, &xrVkDevCi, &out.device, &vkResult);
    if (XR_FAILED(r) || vkResult != VK_SUCCESS) {
        LogError("xrCreateVulkanDeviceKHR failed: xr=%d vk=%d", r, vkResult);
        return false;
    }
    LogInfo("Our VkDevice: %p", (void*)out.device);

    vkGetDeviceQueue(out.device, out.queueFamilyIndex, 0, &out.queue);
    LogInfo("Our VkQueue: %p", (void*)out.queue);
    return true;
}

void DestroyOurVulkan(OurVulkanContext& ctx) {
    if (ctx.device != VK_NULL_HANDLE) {
        vkDestroyDevice(ctx.device, nullptr);
        ctx.device = VK_NULL_HANDLE;
    }
    if (ctx.instance != VK_NULL_HANDLE) {
        vkDestroyInstance(ctx.instance, nullptr);
        ctx.instance = VK_NULL_HANDLE;
    }
}
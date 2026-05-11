module;
#include <d3d9.h>
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <d3d9_vk_interop.h>

export module VR.DXVKInterop;

import VR.Log;
import comvars;

export struct VulkanDeviceHandles {
    VkInstance       instance         = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice   = VK_NULL_HANDLE;
    VkDevice         device           = VK_NULL_HANDLE;
    VkQueue          queue            = VK_NULL_HANDLE;
    uint32_t         queueFamilyIndex = 0;
    uint32_t         queueIndex       = 0;
    ID3D9VkInteropDevice* interopDevice = nullptr; 
};

export bool ExtractVulkanHandles(VulkanDeviceHandles& out) {
    auto* d3d9Device = rage::grcDevice::GetD3DDevice();
    if (!d3d9Device) {
        LogError("D3D9 device is null - called too early?");
        return false;
    }

    HRESULT hr = d3d9Device->QueryInterface(
        __uuidof(ID3D9VkInteropDevice),
        reinterpret_cast<void**>(&out.interopDevice));

    if (FAILED(hr) || !out.interopDevice) {
        LogError("ID3D9VkInteropDevice not available (hr=0x%08X). "
                 "Set FusionFix graphics API to DXVK/Vulkan.", hr);
        return false;
    }

    out.interopDevice->GetVulkanHandles(&out.instance, &out.physicalDevice, &out.device);
    out.interopDevice->GetSubmissionQueue(&out.queue, &out.queueIndex, &out.queueFamilyIndex);
    //interop->Release(); // DO NOT Release() — keep the reference so we can call Lock/UnlockDevice later

    LogInfo("DXVK Vulkan handles extracted:");
    LogInfo("  VkInstance:       %p", (void*)out.instance);
    LogInfo("  VkPhysicalDevice: %p", (void*)out.physicalDevice);
    LogInfo("  VkDevice:         %p", (void*)out.device);
    LogInfo("  VkQueue:          %p (family %u, index %u)",
            (void*)out.queue, out.queueFamilyIndex, out.queueIndex);

    return true;
}
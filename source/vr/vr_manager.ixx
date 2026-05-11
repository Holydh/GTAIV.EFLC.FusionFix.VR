// vr_manager.ixx
module;

#include <common.hxx>

export module VR.Manager;

import common;
import VR.Log;
import VR.OpenXRSession; // defines class OpenXRSession
import VR.DXVKInterop;

namespace {
    OpenXRSession g_VRSession;

	void VRMod_EarlyInit() {
		VRLog_Init();
		LogInfo("VR mod loading (early init)...");
		if (!g_VRSession.InitializeInstance()) {
			LogError("OpenXR init failed - VR disabled");
			return;
		}
		LogInfo("OpenXR ready");
	}

	void VRMod_GraphicsInit() {
		LogInfo("VR mod graphics init...");
		if (!g_VRSession.IsAvailable()) {
			LogInfo("OpenXR not available - skipping graphics init");
			return;
		}

		VulkanDeviceHandles vk;
		if (!ExtractVulkanHandles(vk)) {
			LogError("Failed to extract Vulkan handles - VR disabled");
			return;
		}
		
		if (!g_VRSession.CreateSession(vk)) {
        LogError("Failed to create OpenXR session - VR disabled");
        return;
    }
	}

    void VRMod_Shutdown() {
        LogInfo("VR mod shutting down");
        g_VRSession.Shutdown();
        VRLog_Shutdown();
    }

    struct VRBootstrap {
        VRBootstrap() {
            FusionFix::onInitEvent()      += []() { VRMod_EarlyInit(); };  // logging + OpenXR instance
            FusionFix::onGameInitEvent()  += []() { VRMod_GraphicsInit();};  // DXVK extraction + session	
			FusionFix::onEndScene() += []() { g_VRSession.PollEvents();};
			FusionFix::onShutdownEvent() += []() { VRMod_Shutdown(); };
		}
    };

    VRBootstrap g_vrBootstrap;
}
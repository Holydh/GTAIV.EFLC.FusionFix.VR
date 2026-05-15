// vr_manager.ixx
module;

//#include <common.hxx>

export module VR.Manager;

import common;
import VR.Log;
import VR.DXVKInterop;

namespace {

	void VRMod_EarlyInit() {
		VRLog_Init();
		LogInfo("VR mod loading (early init)...");

	}

    void VRMod_Shutdown() {
        LogInfo("VR mod shutting down");
        VRLog_Shutdown();
    }

    struct VRBootstrap {
        VRBootstrap() {
            FusionFix::onInitEvent()      += []() { VRMod_EarlyInit(); };
            FusionFix::onGameInitEvent()  += []() { LogInfo("onGameInitEvent"); }; 
			FusionFix::onEndScene() += []() { LogInfo("onEndScene"); };
			FusionFix::onShutdownEvent() += []() { VRMod_Shutdown(); };
		}
    };

    VRBootstrap g_vrBootstrap;
}
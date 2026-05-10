// vr_manager.ixx
module;

export module VR.Manager;

import common;
import VR.Log;
import VR.OpenXRSession; // defines class OpenXRSession

namespace {
    OpenXRSession g_VRSession;

    void VRMod_Init() {
        VRLog_Init();
        LogInfo("VR mod loading...");
        if (g_VRSession.InitializeInstance()) {
            LogInfo("OpenXR ready");
        } else {
            LogError("OpenXR init failed - VR disabled");
        }
    }

    void VRMod_Shutdown() {
        LogInfo("VR mod shutting down");
        g_VRSession.Shutdown();
        VRLog_Shutdown();
    }

    struct VRBootstrap {
        VRBootstrap() {
            FusionFix::onInitEvent()     += []() { VRMod_Init(); };
            FusionFix::onShutdownEvent() += []() { VRMod_Shutdown(); };
        }
    };

    VRBootstrap g_vrBootstrap;
}
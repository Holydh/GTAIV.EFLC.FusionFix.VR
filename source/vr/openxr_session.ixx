// in vr/openxr_session.ixx
module;
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
export module VR.OpenXRSession;
import VR.Log;

export class OpenXRSession {
    XrInstance instance;
    XrSession session;
public:
    bool CreateInstance();
};

bool OpenXRSession::CreateInstance() {
    LogInfo("Enumerating OpenXR layers...");
    
    //uint32_t layerCount = 0;
    //xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    //LogInfo("Found %u API layers", layerCount);
    //
    //// ...
    //
    //XrResult r = xrCreateInstance(&ci, &instance);
    //if (XR_FAILED(r)) {
    //    LogError("xrCreateInstance failed: %d", r);
    //    return false;
    //}
    
    LogInfo("OpenXR instance created successfully");
    return true;
}
// in vr/openxr_session.ixx
module;
#define XR_USE_GRAPHICS_API_VULKAN
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vector>
#include <string>

export module VR.OpenXRSession;
import VR.Log;

// OpenXR returns XrResult for nearly every call. Macro :
#define XR_CHECK(call) do { \
    XrResult _r = (call); \
    if (XR_FAILED(_r)) { \
        char _name[XR_MAX_RESULT_STRING_SIZE]; \
        if (instance != XR_NULL_HANDLE) \
            xrResultToString(instance, _r, _name); \
        else \
            snprintf(_name, sizeof(_name), "XrResult(%d)", _r); \
        LogInfo("OpenXR call failed: " #call " -> %s", _name); \
        return false; \
    } \
} while(0)

export class OpenXRSession {
public:
    bool InitializeInstance();
    void Shutdown();
    
    bool IsAvailable() const { return instance != XR_NULL_HANDLE && systemId != XR_NULL_SYSTEM_ID; }
    const std::string& GetSystemName() const { return systemName; }
private :
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrDebugUtilsMessengerEXT debugMessenger = XR_NULL_HANDLE;
    std::string systemName;
    
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool QuerySystem();
    void LogRuntimeInfo();
};

//bool OpenXRSession::CreateInstance() {
//    LogInfo("Enumerating OpenXR layers...");
//    
//    LogInfo("OpenXR instance created successfully");
//    return true;
//}

bool OpenXRSession::CreateInstance() {
    // Enumerate API layers (validation, etc.)
    uint32_t layerCount = 0;
    xrEnumerateApiLayerProperties(0, &layerCount, nullptr);
    std::vector<XrApiLayerProperties> layers(layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
    xrEnumerateApiLayerProperties(layerCount, &layerCount, layers.data());
    
    LogInfo("Available OpenXR API layers (%u):", layerCount);
    for (const auto& l : layers) {
        LogInfo("  %s (spec %u.%u.%u): %s", 
            l.layerName,
            XR_VERSION_MAJOR(l.specVersion),
            XR_VERSION_MINOR(l.specVersion),
            XR_VERSION_PATCH(l.specVersion),
            l.description);
    }
    
    // Enumerate instance extensions
    uint32_t extCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    std::vector<XrExtensionProperties> exts(extCount, {XR_TYPE_EXTENSION_PROPERTIES});
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, exts.data());
    
    LogInfo("Available OpenXR extensions (%u):", extCount);
    for (const auto& e : exts) {
        LogInfo("  %s (v%u)", e.extensionName, e.extensionVersion);
    }
    
    // Build list of extensions we want
    auto hasExtension = [&](const char* name) {
        for (const auto& e : exts) 
            if (strcmp(e.extensionName, name) == 0) return true;
        return false;
    };
    
    std::vector<const char*> enabledExtensions;
    
    // Vulkan binding (for DXVK path)
    if (hasExtension(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME)) {
        enabledExtensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
        LogInfo("Using Vulkan binding (KHR_vulkan_enable2)");
    } else if (hasExtension(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME)) {
        enabledExtensions.push_back(XR_KHR_VULKAN_ENABLE_EXTENSION_NAME);
        LogInfo("Using Vulkan binding (KHR_vulkan_enable - legacy)");
    } else {
        LogError("No Vulkan binding available — OpenXR runtime doesn't support Vulkan?");
        return false;
    }
    
    // Debug messenger (extremely useful)
    bool hasDebugUtils = hasExtension(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (hasDebugUtils) {
        enabledExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    // Optionally enable validation layer in debug builds
    std::vector<const char*> enabledLayers;
    #ifdef _DEBUG
    for (const auto& l : layers) {
        if (strcmp(l.layerName, "XR_APILAYER_LUNARG_core_validation") == 0) {
            enabledLayers.push_back(l.layerName);
        }
    }
    #endif
    
    // Create instance
    XrInstanceCreateInfo ci{XR_TYPE_INSTANCE_CREATE_INFO};
    strcpy_s(ci.applicationInfo.applicationName, "GTA IV VR");
    ci.applicationInfo.applicationVersion = 1;
    strcpy_s(ci.applicationInfo.engineName, "FusionFix");
    ci.applicationInfo.engineVersion = 1;
    ci.applicationInfo.apiVersion = XR_MAKE_VERSION(1, 0, 0);  // was XR_CURRENT_API_VERSION. Caused runtime to fail
    ci.enabledApiLayerCount = (uint32_t)enabledLayers.size();
    ci.enabledApiLayerNames = enabledLayers.data();
    ci.enabledExtensionCount = (uint32_t)enabledExtensions.size();
    ci.enabledExtensionNames = enabledExtensions.data();
    
    XrResult r = xrCreateInstance(&ci, &instance);
    if (XR_FAILED(r)) {
        char errName[XR_MAX_RESULT_STRING_SIZE] = "unknown";
        // can't use xrResultToString here — needs a valid instance which we don't have
        LogError("xrCreateInstance failed: %d", r);
        return false;
    }
    
    // Log runtime info
    XrInstanceProperties props{XR_TYPE_INSTANCE_PROPERTIES};
    xrGetInstanceProperties(instance, &props);
    LogInfo("OpenXR runtime: %s (v%u.%u.%u)",
        props.runtimeName,
        XR_VERSION_MAJOR(props.runtimeVersion),
        XR_VERSION_MINOR(props.runtimeVersion),
        XR_VERSION_PATCH(props.runtimeVersion));
    
    return true;
}

static XRAPI_ATTR XrBool32 XRAPI_CALL DebugCallback(
    XrDebugUtilsMessageSeverityFlagsEXT severity,
    XrDebugUtilsMessageTypeFlagsEXT types,
    const XrDebugUtilsMessengerCallbackDataEXT* data,
    void* userData)
{
    const char* sevStr = "INFO";
    if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) sevStr = "ERROR";
    else if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) sevStr = "WARN";
    else if (severity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) sevStr = "VERB";
    
    LogInfo("[OpenXR-%s] %s: %s", sevStr, data->functionName, data->message);
    return XR_FALSE;
}

bool OpenXRSession::SetupDebugMessenger() {
    PFN_xrCreateDebugUtilsMessengerEXT createMessenger = nullptr;
    XR_CHECK(xrGetInstanceProcAddr(instance, "xrCreateDebugUtilsMessengerEXT",
        reinterpret_cast<PFN_xrVoidFunction*>(&createMessenger)));
    
    if (!createMessenger) return true; // optional
    
    XrDebugUtilsMessengerCreateInfoEXT ci{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    ci.messageSeverities = 
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    ci.messageTypes = 
        XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
        XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    ci.userCallback = DebugCallback;
    
    XR_CHECK(createMessenger(instance, &ci, &debugMessenger));
    return true;
}

bool OpenXRSession::QuerySystem() {
    XrSystemGetInfo si{XR_TYPE_SYSTEM_GET_INFO};
    si.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    
    XrResult r = xrGetSystem(instance, &si, &systemId);
    if (r == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
        LogWarn("HMD not connected/turned on");
        return false;
    }
    if (XR_FAILED(r)) {
        LogError("xrGetSystem failed: %d", r);
        return false;
    }
    
    XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};
    XR_CHECK(xrGetSystemProperties(instance, systemId, &props));
    
    systemName = props.systemName;
    LogInfo("HMD: %s (vendor %u)", props.systemName, props.vendorId);
    LogInfo("  Max swapchain: %ux%u, layers: %u",
        props.graphicsProperties.maxSwapchainImageWidth,
        props.graphicsProperties.maxSwapchainImageHeight,
        props.graphicsProperties.maxLayerCount);
    LogInfo("  Tracking: orientation=%d, position=%d",
        props.trackingProperties.orientationTracking,
        props.trackingProperties.positionTracking);
    
    // Also enumerate view configurations — confirm stereo is available
    uint32_t viewConfigCount = 0;
    XR_CHECK(xrEnumerateViewConfigurations(instance, systemId, 0, &viewConfigCount, nullptr));
    std::vector<XrViewConfigurationType> viewConfigs(viewConfigCount);
    XR_CHECK(xrEnumerateViewConfigurations(instance, systemId, viewConfigCount,
        &viewConfigCount, viewConfigs.data()));
    
    bool hasStereo = false;
    for (auto vc : viewConfigs) {
        if (vc == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) hasStereo = true;
    }
    LogInfo("Stereo view config: %s", hasStereo ? "available" : "MISSING");
    
    if (!hasStereo) {
        LogError("Runtime doesn't support stereo — cannot proceed");
        return false;
    }
    
    return true;
}

bool OpenXRSession::InitializeInstance() {
    if (!CreateInstance()) return false;
    SetupDebugMessenger(); // non-fatal if it fails
    if (!QuerySystem()) {
        Shutdown();
        return false;
    }
    LogInfo("OpenXR Milestone 1 complete: instance + system ready");
    return true;
}

void OpenXRSession::Shutdown() {
    if (debugMessenger != XR_NULL_HANDLE) {
        PFN_xrDestroyDebugUtilsMessengerEXT destroyMessenger = nullptr;
        xrGetInstanceProcAddr(instance, "xrDestroyDebugUtilsMessengerEXT",
            reinterpret_cast<PFN_xrVoidFunction*>(&destroyMessenger));
        if (destroyMessenger) destroyMessenger(debugMessenger);
        debugMessenger = XR_NULL_HANDLE;
    }
    if (instance != XR_NULL_HANDLE) {
        xrDestroyInstance(instance);
        instance = XR_NULL_HANDLE;
    }
    systemId = XR_NULL_SYSTEM_ID;
}
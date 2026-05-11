module;
#define XR_USE_GRAPHICS_API_VULKAN
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#define VK_NO_PROTOTYPES
#include <openxr/openxr_platform.h>
#include <vector>
#include <string>
#include <thread>
#include <atomic>

export module VR.OpenXRSession;
import VR.Log;
import VR.DXVKInterop;
import VR.VulkanInstance;
import VR.VulkanLoader;

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

    void PollEvents();
    bool IsSessionRunning() const { return sessionRunning; }
    bool ShouldExit() const { return exitRequested; }

    bool CreateSession(/*const VulkanDeviceHandles& vk*/);

    void RunFrame();
private :
    XrInstance instance = XR_NULL_HANDLE;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    XrDebugUtilsMessengerEXT debugMessenger = XR_NULL_HANDLE;
    std::string systemName;

	XrSession session = XR_NULL_HANDLE;
	XrSpace referenceSpace = XR_NULL_HANDLE;
	VulkanDeviceHandles vkHandles;
    OurVulkanContext ourVulkan;

    std::vector<XrViewConfigurationView> viewConfigs;

    XrFrameState lastFrameState{XR_TYPE_FRAME_STATE};

    bool CreateInstance();
    bool SetupDebugMessenger();
    bool QuerySystem();
    bool QueryViewConfiguration();

    XrSessionState sessionState = XR_SESSION_STATE_UNKNOWN;
    bool sessionRunning = false;
    bool exitRequested = false;

    std::thread frameThread;
    std::atomic<bool> stopFrameThread{false};
    void StartFrameLoop();
    void StopFrameLoop();
    void FrameLoopThreadMain();

	struct EyeSwapchain {
		XrSwapchain handle = XR_NULL_HANDLE;
		int32_t width = 0;
		int32_t height = 0;
		int64_t format = 0;
		std::vector<XrSwapchainImageVulkanKHR> images;
	};
	std::vector<EyeSwapchain> eyeSwapchains;
    XrEnvironmentBlendMode primaryBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    bool CreateSwapchains();
    bool QueryBlendMode();
    int64_t SelectColorFormat();
};

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
    StopFrameLoop();

    if (referenceSpace != XR_NULL_HANDLE) {
        xrDestroySpace(referenceSpace);
        referenceSpace = XR_NULL_HANDLE;
    }

	for (auto& eye : eyeSwapchains) {
		if (eye.handle != XR_NULL_HANDLE) {
			xrDestroySwapchain(eye.handle);
		}
	}
	eyeSwapchains.clear();

	vkHandles.interopDevice->Release(); // release our reference to the interop device
    DestroyOurVulkan(ourVulkan);

    if (session != XR_NULL_HANDLE) {
        xrDestroySession(session);
        session = XR_NULL_HANDLE;
    }

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

bool OpenXRSession::CreateSession(/*const VulkanDeviceHandles& vk*/) {
    /*vkHandles = vk;*/
	OurVulkanContext ourVk;
	if (!CreateOurVulkan(instance, systemId, ourVk)) return false;

	// Store ourVk somewhere on the class
	this->ourVulkan = ourVk;

    // 1. Required by spec: query graphics requirements
    PFN_xrGetVulkanGraphicsRequirements2KHR pfnGetReqs = nullptr;
    XR_CHECK(xrGetInstanceProcAddr(instance, "xrGetVulkanGraphicsRequirements2KHR",
        reinterpret_cast<PFN_xrVoidFunction*>(&pfnGetReqs)));
    
    XrGraphicsRequirementsVulkanKHR reqs{XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR};
    XR_CHECK(pfnGetReqs(instance, systemId, &reqs));
    LogInfo("OpenXR Vulkan requirements: min v%u.%u.%u, max v%u.%u.%u",
        XR_VERSION_MAJOR(reqs.minApiVersionSupported),
        XR_VERSION_MINOR(reqs.minApiVersionSupported),
        XR_VERSION_PATCH(reqs.minApiVersionSupported),
        XR_VERSION_MAJOR(reqs.maxApiVersionSupported),
        XR_VERSION_MINOR(reqs.maxApiVersionSupported),
        XR_VERSION_PATCH(reqs.maxApiVersionSupported));
    
    // 2. Create session with DXVK Vulkan handles
    //XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    //binding.instance         = vk.instance;
    //binding.physicalDevice   = vk.physicalDevice;
    //binding.device           = vk.device;
    //binding.queueFamilyIndex = vk.queueFamilyIndex;
    //binding.queueIndex       = vk.queueIndex;

        // Session binding now uses OUR Vulkan, not DXVK's
    XrGraphicsBindingVulkanKHR binding{XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR};
    binding.instance         = ourVk.instance;
    binding.physicalDevice   = ourVk.physicalDevice;
    binding.device           = ourVk.device;
    binding.queueFamilyIndex = ourVk.queueFamilyIndex;
    binding.queueIndex       = 0;
    
    XrSessionCreateInfo sci{XR_TYPE_SESSION_CREATE_INFO};
    sci.next     = &binding;
    sci.systemId = systemId;
    
    XR_CHECK(xrCreateSession(instance, &sci, &session));
    LogInfo("OpenXR session created: %p", (void*)session);
    
    // 3. Enumerate reference spaces (informational)
    uint32_t spaceCount = 0;
    XR_CHECK(xrEnumerateReferenceSpaces(session, 0, &spaceCount, nullptr));
    std::vector<XrReferenceSpaceType> spaces(spaceCount);
    XR_CHECK(xrEnumerateReferenceSpaces(session, spaceCount, &spaceCount, spaces.data()));
    
    bool hasLocal = false, hasStage = false, hasView = false;
    for (auto s : spaces) {
        if (s == XR_REFERENCE_SPACE_TYPE_LOCAL) hasLocal = true;
        if (s == XR_REFERENCE_SPACE_TYPE_STAGE) hasStage = true;
        if (s == XR_REFERENCE_SPACE_TYPE_VIEW)  hasView  = true;
    }
    LogInfo("Reference spaces: LOCAL=%d STAGE=%d VIEW=%d", hasLocal, hasStage, hasView);
    
    if (!hasLocal) {
        LogError("LOCAL reference space not supported - cannot proceed");
        return false;
    }
    
    // 4. Create LOCAL reference space at identity pose
    XrReferenceSpaceCreateInfo rsci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    rsci.referenceSpaceType   = XR_REFERENCE_SPACE_TYPE_LOCAL;
    rsci.poseInReferenceSpace = XrPosef{
        {0.0f, 0.0f, 0.0f, 1.0f},  // orientation (quaternion identity)
        {0.0f, 0.0f, 0.0f}          // position
    };
    
    XR_CHECK(xrCreateReferenceSpace(session, &rsci, &referenceSpace));
    LogInfo("Reference space created (LOCAL): %p", (void*)referenceSpace);

	if (!QueryViewConfiguration()) {
		LogError("Expected 2 views for stereo");
		return false;
	}

	if (!QueryBlendMode()) {
		LogError("No blend modes available");
		return false;
	}

    StartFrameLoop();

    LogInfo("OpenXR Milestone 2 complete: session + reference space ready");
    return true;
}

bool OpenXRSession::QueryViewConfiguration() {
    uint32_t viewCount = 0;
    XR_CHECK(xrEnumerateViewConfigurationViews(instance, systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr));
    
    viewConfigs.assign(viewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
    XR_CHECK(xrEnumerateViewConfigurationViews(instance, systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, viewConfigs.data()));
    
    LogInfo("View configuration (stereo, %u views):", viewCount);
    for (uint32_t i = 0; i < viewCount; ++i) {
        const auto& v = viewConfigs[i];
        LogInfo("  Eye %u: recommended %ux%u (max %ux%u), samples %u (max %u)",
            i,
            v.recommendedImageRectWidth, v.recommendedImageRectHeight,
            v.maxImageRectWidth, v.maxImageRectHeight,
            v.recommendedSwapchainSampleCount, v.maxSwapchainSampleCount);
    }
    
    return viewCount == 2;
}

void OpenXRSession::PollEvents() {
    if (instance == XR_NULL_HANDLE) return;
    
    XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
    
    while (xrPollEvent(instance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto* e = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                LogInfo("Session state: %d -> %d", sessionState, e->state);
                sessionState = e->state;
                
                switch (sessionState) {
                    case XR_SESSION_STATE_READY: {
                        XrSessionBeginInfo bi{XR_TYPE_SESSION_BEGIN_INFO};
                        bi.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                        XrResult r = xrBeginSession(session, &bi);
                        if (XR_FAILED(r)) {
                            LogError("xrBeginSession failed: %d", r);
                        } else {
                            sessionRunning = true;
                            LogInfo("Session begun (running)");
                        }
                        break;
                    }
                    case XR_SESSION_STATE_STOPPING: {
                        sessionRunning = false;
                        XrResult r = xrEndSession(session);
                        if (XR_FAILED(r)) LogError("xrEndSession failed: %d", r);
                        else LogInfo("Session ended");
                        break;
                    }
                    case XR_SESSION_STATE_EXITING:
                    case XR_SESSION_STATE_LOSS_PENDING:
                        exitRequested = true;
                        break;
                    default:
                        break;
                }
                break;
            }
            
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                LogWarn("OpenXR instance loss pending");
                exitRequested = true;
                break;
            
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                LogInfo("Interaction profile changed");
                break;
            
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                LogInfo("Reference space change pending");
                break;
            
            default:
                LogInfo("Unhandled OpenXR event type: %d", event.type);
                break;
        }
        
        event = {XR_TYPE_EVENT_DATA_BUFFER};
    }
}

void OpenXRSession::RunFrame() {
	static int callCount = 0;
	callCount++;
	if (callCount % 100 == 0) LogInfo("RunFrame call #%d", callCount);

    if (!sessionRunning || session == XR_NULL_HANDLE) return;
    
    XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
    XrFrameState frameState{XR_TYPE_FRAME_STATE};
    XrResult r = xrWaitFrame(session, &waitInfo, &frameState);
    if (XR_FAILED(r)) {
        LogError("xrWaitFrame failed: %d", r);
        return;
    }
    
    static int frameCount = 0;
    if (frameCount < 5) {
        LogInfo("Frame %d: predictedDisplayTime=%lld shouldRender=%d period=%lld",
            frameCount, (long long)frameState.predictedDisplayTime,
            frameState.shouldRender, (long long)frameState.predictedDisplayPeriod);
    }
    frameCount++;
    
    XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
    r = xrBeginFrame(session, &beginInfo);
    if (r == XR_FRAME_DISCARDED) {
        // Still need to call xrEndFrame per spec
        LogWarn("xrBeginFrame returned XR_FRAME_DISCARDED");
    } else if (XR_FAILED(r)) {
        LogError("xrBeginFrame failed: %d (frame %d)", r, frameCount);
        return;
    }
    
    XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
    endInfo.displayTime = frameState.predictedDisplayTime;
    endInfo.environmentBlendMode = primaryBlendMode;
    endInfo.layerCount = 0;
    endInfo.layers = nullptr;

	r = xrEndFrame(session, &endInfo);
	if (r != XR_SUCCESS) {
		char errName[XR_MAX_RESULT_STRING_SIZE] = "?";
		xrResultToString(instance, r, errName);
		LogError("xrEndFrame: %s (displayTime=%lld)", errName,
			(long long)endInfo.displayTime);
	}
}

void OpenXRSession::StartFrameLoop() {
    stopFrameThread = false;
    frameThread = std::thread([this]() { FrameLoopThreadMain(); });
    LogInfo("OpenXR frame thread started");
}

void OpenXRSession::StopFrameLoop() {
    stopFrameThread = true;
    if (frameThread.joinable()) frameThread.join();
    LogInfo("OpenXR frame thread stopped");
}

void OpenXRSession::FrameLoopThreadMain() {
    LogInfo("Frame thread: creating swapchains...");
    if (!CreateSwapchains()) {
        LogError("Frame thread: swapchain creation failed");
        return;
    }
    LogInfo("Frame thread: swapchains ready, entering loop");
    
    while (!stopFrameThread) {
        if (!sessionRunning || stopFrameThread) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        RunFrame();
    }
}

bool OpenXRSession::QueryBlendMode() {
    uint32_t count = 0;
    XR_CHECK(xrEnumerateEnvironmentBlendModes(instance, systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &count, nullptr));
    std::vector<XrEnvironmentBlendMode> modes(count);
    XR_CHECK(xrEnumerateEnvironmentBlendModes(instance, systemId,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, count, &count, modes.data()));
    
    LogInfo("Supported blend modes (%u):", count);
    for (auto m : modes) LogInfo("  mode %d", m);
    
    if (count == 0) return false;
    primaryBlendMode = modes[0];
    LogInfo("Selected blend mode: %d", primaryBlendMode);
    return true;
}

int64_t OpenXRSession::SelectColorFormat() {
    uint32_t count = 0;
    xrEnumerateSwapchainFormats(session, 0, &count, nullptr);
    std::vector<int64_t> formats(count);
    xrEnumerateSwapchainFormats(session, count, &count, formats.data());
    
    LogInfo("Supported swapchain formats (%u):", count);
    for (auto f : formats) LogInfo("  format %lld", (long long)f);
    
    // Prefer common sRGB formats. Match VkFormat values.
    const int64_t preferred[] = {
        VK_FORMAT_R8G8B8A8_SRGB,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_B8G8R8A8_UNORM,
    };
    for (auto p : preferred) {
        for (auto f : formats) {
            if (f == p) {
                LogInfo("Selected color format: %lld", (long long)f);
                return f;
            }
        }
    }
    // Fallback: first available
    return formats.empty() ? 0 : formats[0];
}

bool OpenXRSession::CreateSwapchains() {
    if (viewConfigs.size() != 2) return false;
    
    int64_t format = SelectColorFormat();
    if (format == 0) {
        LogError("No supported swapchain format");
        return false;
    }
    
    eyeSwapchains.resize(2);
    
    for (size_t i = 0; i < 2; ++i) {
        auto& eye = eyeSwapchains[i];
        const auto& vc = viewConfigs[i];

        eye.width  = vc.recommendedImageRectWidth;
        eye.height = vc.recommendedImageRectHeight;
        eye.format = format;
        
        XrSwapchainCreateInfo sci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                         XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT |
                         XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        sci.format       = format;
        sci.sampleCount  = 1;
        sci.width        = eye.width;
        sci.height       = eye.height;
        sci.faceCount    = 1;
        sci.arraySize    = 1;
        sci.mipCount     = 1;

		LogInfo("Creating swapchain for eye %zu...", i);
		XR_CHECK(xrCreateSwapchain(session, &sci, &eye.handle));
		LogInfo("Swapchain handle: %p", (void*)eye.handle);

		uint32_t imageCount = 0;
		LogInfo("Querying image count...");
		XR_CHECK(xrEnumerateSwapchainImages(eye.handle, 0, &imageCount, nullptr));
		LogInfo("Image count: %u", imageCount);

        eye.images.clear();
		eye.images.resize(imageCount);
		for (auto& img : eye.images) {
			img.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
			img.next = nullptr;
			img.image = VK_NULL_HANDLE;
		}

        LogInfo("sizeof(XrSwapchainImageVulkanKHR) = %zu", sizeof(XrSwapchainImageVulkanKHR));
        LogInfo("sizeof(VkImage) = %zu", sizeof(VkImage));

        LogInfo("Address of images[0]: %p", &eye.images[0]);
        LogInfo("eye.images.size() = %zu, capacity = %zu", eye.images.size(), eye.images.capacity());

		LogInfo("Enumerating images...");
		XR_CHECK(xrEnumerateSwapchainImages(eye.handle, imageCount, &imageCount,
			reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data())));
		LogInfo("Eye %zu done", i);

		LogInfo("Eye %zu swapchain: %dx%d, %u images, handle=%p",
            i, eye.width, eye.height, imageCount, (void*)eye.handle);
        for (uint32_t j = 0; j < imageCount; ++j) {
            LogInfo("  image %u: VkImage=%p", j, (void*)eye.images[j].image);
        }
    }
    
    return true;
}
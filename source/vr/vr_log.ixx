module;

#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <mutex>

export module VR.Log;

namespace {
    FILE* g_logFile = nullptr;
    std::mutex g_logMutex;
}

export void VRLog_Init() {
    g_logFile = _fsopen("GTAIV_VR.log", "w", _SH_DENYWR);
    if (g_logFile) {
        fprintf(g_logFile, "=== GTAIV VR Mod Log ===\n");
        fflush(g_logFile);
    }
}

export void VRLog_Shutdown() {
    if (g_logFile) {
        fprintf(g_logFile, "=== Log closed ===\n");
        fclose(g_logFile);
        g_logFile = nullptr;
    }
}

export void VRLog_Write(const char* level, const char* fmt, ...) {
    if (!g_logFile) return;
    
    std::lock_guard<std::mutex> lock(g_logMutex);
    
    // Timestamp
    time_t now = time(nullptr);
    struct tm tm_local;
    localtime_s(&tm_local, &now);
    char timeBuf[16];
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm_local);
    
    fprintf(g_logFile, "[%s] [%s] ", timeBuf, level);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(g_logFile, fmt, args);
    va_end(args);
    
    fprintf(g_logFile, "\n");
    fflush(g_logFile);  // flush every line so logs survive a crash
}

export inline void LogInfo(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    VRLog_Write("INFO", "%s", buf);
}

export inline void LogWarn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    VRLog_Write("WARN", "%s", buf);
}

export inline void LogError(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    VRLog_Write("ERROR", "%s", buf);
}
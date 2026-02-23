#include "mod/util/DebugLogger.h"
#include <fstream>
#include <mutex>
#include <filesystem>
#include "ll/api/mod/NativeMod.h"

namespace origin_mod::util {

namespace {
std::mutex g_mutex;
std::ofstream g_debugFile;

// runtime switch; default depends on build configuration
bool g_enabled
#ifndef NDEBUG
    = true;
#else
    = false;
#endif
;

void ensureOpened() {
    if (g_debugFile.is_open()) {
        return;
    }

    // Prefer the mod's config directory so the path is writable and
    // consistent across environments.  This mirrors how
    // FeatureManager::configPath constructs features.json.
    std::filesystem::path path = "debug.txt";
    if (auto nm = ll::mod::NativeMod::current()) {
        path = nm->getConfigDir() / "debug.txt";
        // ensure the directory exists
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            // fall back to cwd if creation failed
            path = "debug.txt";
        }
    }
    g_debugFile.open(path, std::ios::app);
}
} // namespace

void setDebugEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_enabled = enabled;
}

bool isDebugEnabled() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_enabled;
}

void debugLog(std::string_view msg) {
    if (!isDebugEnabled()) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    ensureOpened();
    if (g_debugFile.is_open()) {
        g_debugFile << msg << '\n';
        g_debugFile.flush();
    }
}

} // namespace origin_mod::util

#include "mod/util/DebugLogger.h"
#include <fstream>
#include <mutex>
#include <filesystem>
#include "ll/api/mod/NativeMod.h"

namespace origin_mod::util {

namespace {
std::mutex g_mutex;
std::ofstream g_debugFile;

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

void debugLog(std::string_view msg) {
#ifndef NDEBUG
    std::lock_guard<std::mutex> lock(g_mutex);
    ensureOpened();
    if (g_debugFile.is_open()) {
        g_debugFile << msg << '\n';
        g_debugFile.flush();
    }
#else
    (void)msg;
#endif
}

} // namespace origin_mod::util

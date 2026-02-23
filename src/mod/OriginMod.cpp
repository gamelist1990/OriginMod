#include "mod/OriginMod.h"
#include "mod/hooks/ChatHook.h"
#include "mod/features/FeatureManager.h"
#include "mod/util/DebugLogger.h"

#include "ll/api/mod/RegisterHelper.h"

namespace origin_mod {

OriginMod& OriginMod::getInstance() {
    static OriginMod instance;
    return instance;
}

ll::mod::NativeMod& OriginMod::getSelf() const {
    return *ll::mod::NativeMod::current();
}

bool OriginMod::load() {
    // Debugを有効にする
    origin_mod::util::setDebugEnabled(true);

    getSelf().getLogger().info("OriginMod Loading...");

    return true;
}

bool OriginMod::enable() {
    getSelf().getLogger().info("OriginMod Enabling...");

    try {
        // フィーチャーマネージャを初期化
        auto& featureManager = features::FeatureManager::instance();
        featureManager.initialize(*this);

        // チャットフックを初期化（コマンドシステムも含む）
        hooks::initializeChatHook(*this);

        getSelf().getLogger().info("OriginMod successfully enabled!");
        return true;

    } catch (const std::exception& e) {
        getSelf().getLogger().error("Failed to enable OriginMod: {}", e.what());
        return false;
    }
}

bool OriginMod::disable() {
    getSelf().getLogger().info("OriginMod Disabling...");

    try {
        // チャットフックを終了
        hooks::shutdownChatHook();

        // フィーチャーマネージャを終了
        auto& featureManager = features::FeatureManager::instance();
        featureManager.shutdown(*this);

        getSelf().getLogger().info("OriginMod successfully disabled!");
        return true;

    } catch (const std::exception& e) {
        getSelf().getLogger().error("Failed to disable OriginMod: {}", e.what());
        return false;
    }
}

} // namespace origin_mod

LL_REGISTER_MOD(origin_mod::OriginMod, origin_mod::OriginMod::getInstance());
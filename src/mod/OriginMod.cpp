#include "mod/OriginMod.h"
#include "mod/hooks/ChatHook.h"
#include "mod/features/FeatureManager.h"
#include "mod/config/ConfigManager.h"
#include "mod/config/MessageConfig.h"

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
    getSelf().getLogger().info("OriginMod Loading...");
    return true;
}

bool OriginMod::enable() {
    getSelf().getLogger().info("OriginMod Enabling...");

    try {
        // 1. ConfigManagerを初期化
        auto& configManager = config::ConfigManager::instance();
        configManager.initialize(*this);

        // 2. MessageConfigを初期化
        auto& messageConfig = config::MessageConfig::instance();
        messageConfig.initialize(*this);

        // 3. フィーチャーマネージャを初期化
        auto& featureManager = features::FeatureManager::instance();
        featureManager.initialize(*this);

        // 5. チャットフックを初期化（コマンドシステムも含む）
        hooks::initializeChatHook(*this);

        getSelf().getLogger().info("OriginMod successfully enabled! (World Event Architecture)");
        return true;

    } catch (const std::exception& e) {
        getSelf().getLogger().error("Failed to enable OriginMod: {}", e.what());
        return false;
    }
}

bool OriginMod::disable() {
    getSelf().getLogger().info("OriginMod Disabling...");

    try {
        // 2. チャットフックを終了
        hooks::shutdownChatHook();

        // 3. フィーチャーマネージャを終了
        auto& featureManager = features::FeatureManager::instance();
        featureManager.shutdown(*this);

        // 4. MessageConfigを終了
        auto& messageConfig = config::MessageConfig::instance();
        messageConfig.shutdown();

        // 5. ConfigManagerを終了
        auto& configManager = config::ConfigManager::instance();
        configManager.shutdown();

        getSelf().getLogger().info("OriginMod successfully disabled! (World Event Architecture)");
        return true;

    } catch (const std::exception& e) {
        getSelf().getLogger().error("Failed to disable OriginMod: {}", e.what());
        return false;
    }
}

} // namespace origin_mod

LL_REGISTER_MOD(origin_mod::OriginMod, origin_mod::OriginMod::getInstance());
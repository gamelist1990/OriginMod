#include "mod/commands/config/ConfigCommand.h"
#include "mod/api/Player.h"
#include "mod/config/ConfigManager.h"
#include "mod/config/MessageConfig.h"
#include "mod/features/FeatureManager.h"
#include "mod/OriginMod.h"

#include <fmt/format.h>

namespace origin_mod::commands::config {

void executeConfig(OriginMod& mod, const std::vector<std::string>& args) {
    origin_mod::api::Player player{mod};

    if (args.size() >= 1 && args[0] == "reload") {
        // 設定ファイルの再読み込み
        if (args.size() >= 2) {
            // 特定ファイルのリロード
            const auto& fileName = args[1];
            auto& configManager = origin_mod::config::ConfigManager::instance();

            if (fileName == "messages" || fileName == "messages.json") {
                auto& messageConfig = origin_mod::config::MessageConfig::instance();
                messageConfig.reloadConfig();
                player.localSendMessage("§amessages.json をリロードしました");
            } else if (fileName == "features" || fileName == "features.json") {
                auto& featureManager = origin_mod::features::FeatureManager::instance();
                featureManager.reloadConfig(mod);
                player.localSendMessage("§afeatures.json をリロードしました");
            } else {
                configManager.reloadConfig(fileName);
                player.localSendMessage(fmt::format("§a{} をリロードしました", fileName));
            }
        } else {
            // 全設定ファイルのリロード
            auto& configManager = origin_mod::config::ConfigManager::instance();
            auto& messageConfig = origin_mod::config::MessageConfig::instance();
            auto& featureManager = origin_mod::features::FeatureManager::instance();

            configManager.reloadAllConfigs();
            messageConfig.reloadConfig();
            featureManager.reloadConfig(mod);

            player.localSendMessage("§a全ての設定ファイルをリロードしました");
        }

    } else if (args.size() >= 1 && args[0] == "status") {
        // 設定システムの状態表示
        auto& configManager = origin_mod::config::ConfigManager::instance();
        auto& messageConfig = origin_mod::config::MessageConfig::instance();
        auto& featureManager = origin_mod::features::FeatureManager::instance();

        player.localSendMessage("=== 設定システム状態 ===");
        player.localSendMessage(fmt::format("§7ConfigManager: {}",
            configManager.isInitialized() ? "初期化済み" : "未初期化"));
        player.localSendMessage(fmt::format("§7MessageConfig: {}",
            messageConfig.isInitialized() ? "初期化済み" : "未初期化"));
        player.localSendMessage(fmt::format("§7FeatureManager: {}",
            featureManager.isInitialized() ? "初期化済み" : "未初期化"));

        const auto& killggConfig = messageConfig.getKillGGConfig();
        const auto& autoggConfig = messageConfig.getAutoGGConfig();

        player.localSendMessage(fmt::format("§7KillGG: {} ({} messages, {}ms cooldown)",
            killggConfig.enabled ? "有効" : "無効",
            killggConfig.messages.size(),
            killggConfig.cooldown));

        player.localSendMessage(fmt::format("§7AutoGG: {} ({} messages, {}-{}ms delay)",
            autoggConfig.enabled ? "有効" : "無効",
            autoggConfig.messages.size(),
            autoggConfig.minDelay,
            autoggConfig.maxDelay));

        player.localSendMessage(fmt::format("§7Config Directory: {}",
            configManager.getConfigDir().string()));

    } else if (args.size() >= 1 && args[0] == "test") {
        // 設定テスト
        if (args.size() >= 2 && args[1] == "killgg") {
            auto& messageConfig = origin_mod::config::MessageConfig::instance();
            auto message = messageConfig.getRandomKillGGMessage("TestPlayer");
            if (message.has_value()) {
                player.localSendMessage(fmt::format("§eKillGG Test: {}", message.value()));
            } else {
                player.localSendMessage("§cKillGG: メッセージが取得できませんでした");
            }
        } else if (args.size() >= 2 && args[1] == "autogg") {
            auto& messageConfig = origin_mod::config::MessageConfig::instance();
            auto message = messageConfig.getRandomAutoGGMessage();
            if (message.has_value()) {
                player.localSendMessage(fmt::format("§eAutoGG Test: {}", message.value()));
            } else {
                player.localSendMessage("§cAutoGG: メッセージが取得できませんでした");
            }
        } else {
            player.localSendMessage("§7使用法: /config test <killgg|autogg>");
        }

    } else {
        // ヘルプ表示
        player.localSendMessage("=== 設定管理コマンド ===");
        player.localSendMessage("§7使用法:");
        player.localSendMessage("§7  /config reload [file] - 設定ファイルをリロード");
        player.localSendMessage("§7  /config status - 設定システムの状態表示");
        player.localSendMessage("§7  /config test <killgg|autogg> - メッセージのテスト");
        player.localSendMessage("§7");
        player.localSendMessage("§7利用可能なファイル:");
        player.localSendMessage("§7  messages.json - KillGG/AutoGGメッセージ設定");
        player.localSendMessage("§7  features.json - 機能の有効/無効設定");
    }
}

} // namespace origin_mod::commands::config
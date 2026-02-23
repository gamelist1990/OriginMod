#include "mod/config/MessageConfig.h"
#include "mod/config/ConfigManager.h"
#include "mod/OriginMod.h"

#include <fmt/format.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace origin_mod::config {

MessageConfig& MessageConfig::instance() {
    static MessageConfig g;
    return g;
}

void MessageConfig::initialize(origin_mod::OriginMod& mod) {
    if (mInitialized) return;

    mMod = &mod;

    // デフォルト設定を確保
    ensureDefaultConfig();

    // 設定を読み込み
    loadFromConfigManager();

    mInitialized = true;

    mod.getSelf().getLogger().info("MessageConfig initialized - KillGG: {} messages, AutoGG: {} messages",
        mKillGGConfig.messages.size(), mAutoGGConfig.messages.size());
}

void MessageConfig::shutdown() {
    if (!mInitialized) return;

    mMod = nullptr;
    mInitialized = false;
}

std::optional<std::string> MessageConfig::getRandomKillGGMessage(const std::string& targetName) const {
    if (!mInitialized || !mKillGGConfig.enabled || mKillGGConfig.messages.empty()) {
        return std::nullopt;
    }

    // ランダムなテンプレートを選択
    std::uniform_int_distribution<size_t> dist(0, mKillGGConfig.messages.size() - 1);
    const auto& templateStr = mKillGGConfig.messages[dist(mRng)];

    try {
        // {} を targetName で置換
        auto message = fmt::format(fmt::runtime(templateStr), targetName);
        return message;
    } catch (const std::exception& e) {
        if (mMod) {
            mMod->getSelf().getLogger().warn("Failed to format KillGG message '{}': {}", templateStr, e.what());
        }
        return templateStr; // フォーマットに失敗した場合は元の文字列を返す
    }
}

std::optional<std::string> MessageConfig::getRandomAutoGGMessage() const {
    if (!mInitialized || !mAutoGGConfig.enabled || mAutoGGConfig.messages.empty()) {
        return std::nullopt;
    }

    // ランダムなメッセージを選択
    std::uniform_int_distribution<size_t> dist(0, mAutoGGConfig.messages.size() - 1);
    return mAutoGGConfig.messages[dist(mRng)];
}

void MessageConfig::reloadConfig() {
    if (!mInitialized) return;

    // ConfigManagerに再読み込みを依頼
    auto& configManager = ConfigManager::instance();
    configManager.reloadConfig("messages.json");

    // 設定を再読み込み
    loadFromConfigManager();

    if (mMod) {
        mMod->getSelf().getLogger().info("MessageConfig reloaded");
    }
}

void MessageConfig::saveConfig() {
    if (!mInitialized) return;

    auto& configManager = ConfigManager::instance();

    json data;
    data["killgg"]["enabled"] = mKillGGConfig.enabled;
    data["killgg"]["messages"] = mKillGGConfig.messages;
    data["killgg"]["cooldown"] = mKillGGConfig.cooldown;

    data["autogg"]["enabled"] = mAutoGGConfig.enabled;
    data["autogg"]["messages"] = mAutoGGConfig.messages;
    data["autogg"]["minDelay"] = mAutoGGConfig.minDelay;
    data["autogg"]["maxDelay"] = mAutoGGConfig.maxDelay;

    configManager.saveConfig("messages.json", data);
}

void MessageConfig::loadFromConfigManager() {
    auto& configManager = ConfigManager::instance();
    auto config = configManager.loadConfig("messages.json");

    if (!config.has_value()) {
        if (mMod) {
            mMod->getSelf().getLogger().warn("Failed to load messages.json, using defaults");
        }
        return;
    }

    const auto& data = config.value();

    try {
        // KillGG設定
        if (data.contains("killgg")) {
            const auto& killgg = data["killgg"];

            if (killgg.contains("enabled") && killgg["enabled"].is_boolean()) {
                mKillGGConfig.enabled = killgg["enabled"];
            }

            if (killgg.contains("messages") && killgg["messages"].is_array()) {
                mKillGGConfig.messages.clear();
                for (const auto& msg : killgg["messages"]) {
                    if (msg.is_string()) {
                        mKillGGConfig.messages.push_back(msg);
                    }
                }
            }

            if (killgg.contains("cooldown") && killgg["cooldown"].is_number()) {
                mKillGGConfig.cooldown = killgg["cooldown"];
            }
        }

        // AutoGG設定
        if (data.contains("autogg")) {
            const auto& autogg = data["autogg"];

            if (autogg.contains("enabled") && autogg["enabled"].is_boolean()) {
                mAutoGGConfig.enabled = autogg["enabled"];
            }

            if (autogg.contains("messages") && autogg["messages"].is_array()) {
                mAutoGGConfig.messages.clear();
                for (const auto& msg : autogg["messages"]) {
                    if (msg.is_string()) {
                        mAutoGGConfig.messages.push_back(msg);
                    }
                }
            }

            if (autogg.contains("minDelay") && autogg["minDelay"].is_number()) {
                mAutoGGConfig.minDelay = autogg["minDelay"];
            }

            if (autogg.contains("maxDelay") && autogg["maxDelay"].is_number()) {
                mAutoGGConfig.maxDelay = autogg["maxDelay"];
            }
        }

    } catch (const std::exception& e) {
        if (mMod) {
            mMod->getSelf().getLogger().error("Failed to parse message config: {}", e.what());
        }
    }
}

void MessageConfig::ensureDefaultConfig() {
    auto& configManager = ConfigManager::instance();
    auto defaultConfig = createDefaultConfig();
    configManager.ensureDefaultConfig("messages.json", defaultConfig);
}

json MessageConfig::createDefaultConfig() const {
    json config;

    // KillGGデフォルトメッセージ（既存のものから取得）
    config["killgg"]["enabled"] = true;
    config["killgg"]["cooldown"] = 800;
    config["killgg"]["messages"] = json::array({
        "{}さんお疲れ～GG",
        "{}さんナイスファイト！GG",
        "{}さん乙！GG",
        "{}さんGG～",
        "{}さんおつでした！",
        "{}さんありがと～GG",
        "{}さんお疲れ様！GG",
        "{}さんGood Game!",
        "{}さんGGWP!",
        "{}さんドンマイ！GG",
        "{}さんお見事！GG",
        "{}さん対戦ありがとうございました！",
        "{}さんまたよろしく～GG",
        "{}さんおつGG",
        "{}さんファイト！GG",
        "{}さんお疲れっした！GG",
        "{}さん(｀・ω・´)b GG",
        "{}さんいい勝負だった！GG",
        "{}さんおつかれ～",
        "{}さんGG ありがとう！"
    });

    // AutoGGデフォルトメッセージ（既存のものから取得）
    config["autogg"]["enabled"] = true;
    config["autogg"]["minDelay"] = 1000;
    config["autogg"]["maxDelay"] = 2000;
    config["autogg"]["messages"] = json::array({
        "gg",
        "ggwp",
        "Good game!",
        "Well played!",
        "gg wp",
        "Good Game Well Played!",
        "ナイスゲーム！",
        "お疲れ様でした！",
        "良い試合でした！",
        "ggでした！",
        "おつかれ！",
        "ナイスファイト！",
        "Good match!",
        "wp gg",
        "gg all",
        "みなさんお疲れ様！",
        "楽しかった！gg",
        "また遊びましょう！",
        "Great game everyone!",
        "Thanks for the game!"
    });

    return config;
}

} // namespace origin_mod::config
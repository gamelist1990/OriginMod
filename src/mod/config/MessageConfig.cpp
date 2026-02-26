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

    mod.getSelf().getLogger().info("MessageConfig initialized - AutoGG: {} messages, TopKillerTaunt: {} messages",
        mAutoGGConfig.messages.size(), mTopKillerTauntConfig.messages.size());
}

void MessageConfig::shutdown() {
    if (!mInitialized) return;

    mMod = nullptr;
    mInitialized = false;
}

// getRandomKillGGMessage 削除済み

std::optional<std::string> MessageConfig::getRandomAutoGGMessage() const {
    if (!mInitialized || !mAutoGGConfig.enabled || mAutoGGConfig.messages.empty()) {
        return std::nullopt;
    }

    // ランダムなメッセージを選択
    std::uniform_int_distribution<size_t> dist(0, mAutoGGConfig.messages.size() - 1);
    return mAutoGGConfig.messages[dist(mRng)];
}

std::optional<std::string> MessageConfig::getRandomTopKillerTauntMessage(const std::string& targetName) const {
    if (!mInitialized || !mTopKillerTauntConfig.enabled || mTopKillerTauntConfig.messages.empty()) {
        return std::nullopt;
    }

    // ランダムなテンプレートを選択
    std::uniform_int_distribution<size_t> dist(0, mTopKillerTauntConfig.messages.size() - 1);
    const auto& templateStr = mTopKillerTauntConfig.messages[dist(mRng)];

    try {
        // {} を targetName で置換
        auto message = fmt::format(fmt::runtime(templateStr), targetName);
        return message;
    } catch (const std::exception& e) {
        if (mMod) {
            mMod->getSelf().getLogger().warn("Failed to format TopKillerTaunt message '{}': {}", templateStr, e.what());
        }
        return templateStr; // フォーマットに失敗した場合は元の文字列を返す
    }
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
    // KillGG保存処理削除

    data["autogg"]["enabled"] = mAutoGGConfig.enabled;
    data["autogg"]["messages"] = mAutoGGConfig.messages;
    data["autogg"]["minDelay"] = mAutoGGConfig.minDelay;
    data["autogg"]["maxDelay"] = mAutoGGConfig.maxDelay;

    data["topkillertaunt"]["enabled"] = mTopKillerTauntConfig.enabled;
    data["topkillertaunt"]["messages"] = mTopKillerTauntConfig.messages;
    data["topkillertaunt"]["cooldown"] = mTopKillerTauntConfig.cooldown;

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
        // KillGG設定削除済み

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

        // TopKillerTaunt設定
        if (data.contains("topkillertaunt")) {
            const auto& taunt = data["topkillertaunt"];

            if (taunt.contains("enabled") && taunt["enabled"].is_boolean()) {
                mTopKillerTauntConfig.enabled = taunt["enabled"];
            }

            if (taunt.contains("messages") && taunt["messages"].is_array()) {
                mTopKillerTauntConfig.messages.clear();
                for (const auto& msg : taunt["messages"]) {
                    if (msg.is_string()) {
                        mTopKillerTauntConfig.messages.push_back(msg);
                    }
                }
            }

            if (taunt.contains("cooldown") && taunt["cooldown"].is_number()) {
                mTopKillerTauntConfig.cooldown = taunt["cooldown"];
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

    // KillGGデフォルト削除済み

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

    // TopKillerTauntデフォルトメッセージ
    config["topkillertaunt"]["enabled"] = true;
    config["topkillertaunt"]["cooldown"] = 3000;
    config["topkillertaunt"]["messages"] = json::array({
        "{}を倒せ～～！",
        "{}に負けるな！",
        "{}、調子に乗ってるぞ！",
        "{}を止めろ！",
        "{}強すぎ！でも負けない！",
        "{}、覚えてろよ！",
        "{}に復讐だ！",
        "{}め、今度こそ倒す！",
        "{}、待ってろよ！",
        "{}には負けられない！",
        "{}を狙え！",
        "{}、次はこっちの番だ！",
        "{}、いい気になるなよ！",
        "{}を打倒せよ！",
        "{}、勝負だ！"
    });

    return config;
}

} // namespace origin_mod::config
#pragma once

#include <string>
#include <vector>
#include <random>
#include <optional>

#include <nlohmann/json.hpp>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::config {

/**
 * メッセージ設定クラス
 * KillGG、AutoGG機能のメッセージ管理を統一化
 */
class MessageConfig {
public:
    static MessageConfig& instance();

    // 初期化・終了処理
    void initialize(origin_mod::OriginMod& mod);
    void shutdown();

    /**
     * KillGG設定
     */
    struct KillGGConfig {
        bool enabled{true};
        std::vector<std::string> messages;
        int cooldown{800}; // ミリ秒
    };

    /**
     * AutoGG設定
     */
    struct AutoGGConfig {
        bool enabled{true};
        std::vector<std::string> messages;
        int minDelay{1000}; // ミリ秒
        int maxDelay{2000}; // ミリ秒
    };

    // 設定取得メソッド
    [[nodiscard]] const KillGGConfig& getKillGGConfig() const { return mKillGGConfig; }
    [[nodiscard]] const AutoGGConfig& getAutoGGConfig() const { return mAutoGGConfig; }

    // ランダムメッセージ取得
    std::optional<std::string> getRandomKillGGMessage(const std::string& targetName = "相手") const;
    std::optional<std::string> getRandomAutoGGMessage() const;

    // 設定の再読み込み
    void reloadConfig();

    // 設定の保存
    void saveConfig();

    [[nodiscard]] bool isInitialized() const noexcept { return mInitialized; }

private:
    MessageConfig() = default;

    // 設定データ
    KillGGConfig mKillGGConfig;
    AutoGGConfig mAutoGGConfig;

    // OriginModインスタンスの参照
    origin_mod::OriginMod* mMod{nullptr};
    bool mInitialized{false};

    // 乱数生成器
    mutable std::mt19937 mRng{std::random_device{}()};

    // 内部メソッド
    void loadFromConfigManager();
    void ensureDefaultConfig();
    nlohmann::json createDefaultConfig() const;
};

} // namespace origin_mod::config
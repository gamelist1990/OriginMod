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

    // KillGG機能は削除済み

    /**
     * AutoGG設定
     */
    struct AutoGGConfig {
        bool enabled{true};
        std::vector<std::string> messages;
        int minDelay{1000}; // ミリ秒
        int maxDelay{2000}; // ミリ秒
    };

    /**
     * Top Killer Taunt設定
     */
    struct TopKillerTauntConfig {
        bool enabled{true};
        std::vector<std::string> messages;
        int cooldown{3000}; // ミリ秒
    };

    // 設定取得メソッド
    // getKillGGConfig() 削除済み
    [[nodiscard]] const AutoGGConfig& getAutoGGConfig() const { return mAutoGGConfig; }
    [[nodiscard]] const TopKillerTauntConfig& getTopKillerTauntConfig() const { return mTopKillerTauntConfig; }

    // ランダムメッセージ取得
    // getRandomKillGGMessage 削除済み
    std::optional<std::string> getRandomAutoGGMessage() const;
    std::optional<std::string> getRandomTopKillerTauntMessage(const std::string& targetName = "やつ") const;

    // 設定の再読み込み
    void reloadConfig();

    // 設定の保存
    void saveConfig();

    [[nodiscard]] bool isInitialized() const noexcept { return mInitialized; }

private:
    MessageConfig() = default;

    // 設定データ
    // mKillGGConfig 削除済み
    AutoGGConfig mAutoGGConfig;
    TopKillerTauntConfig mTopKillerTauntConfig;

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
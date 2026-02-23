#include <array>
#include <chrono>
#include <random>
#include <regex>
#include <string>
#include <string_view>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

// 20種類のAutoGGメッセージ
constexpr std::array<std::string_view, 20> kAutoGgMessages{
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
};

// ゲーム終了を検出するキーワード
constexpr std::array<std::string_view, 26> kGameEndKeywords{
    // 日本語パターン
    "ゲーム終了",
    "試合終了",
    "勝利",
    "敗北",
    "勝ち",
    "負け",
    "優勝",
    "ゲームオーバー",
    "GAME OVER",
    "Game Over",

    // 英語パターン
    "won the game",
    "has won the game",
    "You won",
    "You lost",
    "Victory",
    "Defeat",
    "Winner",
    "Game End",
    "Match End",
    "Champions",
    "Champion",

    // サーバー固有パターン
    "§a won the game!",        // CubeCraft
    "§a has won the game!",    // Lifeboat
    "§aYou Win!",             // Mineville
    "§cGame Over!",           // Mineville
    "Team§r§a won"            // Galaxite
};

class AutoGgFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "autogg"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Auto GG"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "ゲーム終了時に自動的にGGメッセージを送信します";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) return;

        // World onReceiveChatにリスナーを登録（サーバーカスタムメッセージ対応）
        auto& world = api::World::instance();
        mChatListenerId = world.beforeEvents().chatSend.subscribe(
            [this, &mod](api::ChatSendEvent& event) {
                // onReceiveChat経由でサーバーメッセージを処理
                onChatReceive(mod, event.message);
            }
        );

        mEnabled = true;
        mPlayer = std::make_unique<api::Player>(mod);

        mod.getSelf().getLogger().info("AutoGG enabled (onReceiveChat mode)");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        if (!mEnabled) return;

        // リスナーを解除
        if (mChatListenerId != 0) {
            auto& world = api::World::instance();
            world.beforeEvents().chatSend.unsubscribe(mChatListenerId);
            mChatListenerId = 0;
        }

        mPlayer.reset();
        mLastGgTime = {};
        mEnabled = false;

        mod.getSelf().getLogger().info("AutoGG disabled");
    }

private:
    bool mEnabled{false};
    uint64_t mChatListenerId{0};
    std::unique_ptr<api::Player> mPlayer;
    std::chrono::steady_clock::time_point mLastGgTime{};

    // 色コード除去ユーティリティ
    std::string removeColorCodes(const std::string& text) {
        std::string result;
        result.reserve(text.length());

        for (size_t i = 0; i < text.length(); ++i) {
            if (text[i] == '§' && i + 1 < text.length()) {
                // 色コード（§ + 1文字）をスキップ
                ++i;
                continue;
            }
            result += text[i];
        }
        return result;
    }

    void onChatReceive(origin_mod::OriginMod& mod, const std::string& message) {
        if (!mEnabled || !mPlayer) return;

        // 色コードを除去して判定
        std::string cleanMessage = removeColorCodes(message);

        mod.getSelf().getLogger().debug("AutoGG checking message: '{}' (clean: '{}')",
            message, cleanMessage);

        // ゲーム終了メッセージかどうか判定
        bool isGameEndMessage = false;
        std::string matchedKeyword;

        for (const auto& keyword : kGameEndKeywords) {
            if (message.find(keyword) != std::string::npos ||
                cleanMessage.find(keyword) != std::string::npos) {
                isGameEndMessage = true;
                matchedKeyword = keyword;
                break;
            }
        }

        // 正規表現パターンもチェック
        if (!isGameEndMessage) {
            // "Is The Champion" パターン
            static const std::regex championRegex(R"(Is The §6§l(Chronos|Rush) (Champion|Champions)!)");
            if (std::regex_search(message, championRegex)) {
                isGameEndMessage = true;
                matchedKeyword = "Champion";
            }
        }

        if (!isGameEndMessage) return;

        mod.getSelf().getLogger().info("AutoGG: Game end detected with keyword '{}' in message: '{}'",
            matchedKeyword, message);

        // クールダウンチェック（3秒）
        auto now = std::chrono::steady_clock::now();
        if (now - mLastGgTime < std::chrono::seconds(3)) {
            mod.getSelf().getLogger().debug("AutoGG: Still in cooldown, skipping");
            return;
        }

        // ランダムなGGメッセージを送信
        sendRandomGgMessage(mod);
        mLastGgTime = now;
    }

    void sendRandomGgMessage(origin_mod::OriginMod& mod) {
        // ランダムなメッセージを選択
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, kAutoGgMessages.size() - 1);

        auto message = std::string{kAutoGgMessages[dist(rng)]};

        try {
            // 1-2秒の遅延を追加（よりリアルに見せる）
            std::uniform_int_distribution<int> delayDist(1000, 2000);
            int delayMs = delayDist(rng);

            // TODO: 遅延送信を実装（現在は即座に送信）
            mPlayer->sendMessage(message);
            mod.getSelf().getLogger().info("AutoGG: Sent message: '{}'", message);
        } catch (...) {
            mod.getSelf().getLogger().debug("AutoGG: Failed to send message");
        }
    }
};

} // namespace origin_mod::features

namespace {
void registerAutoGg(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AutoGgFeature>("autogg");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAutoGg);
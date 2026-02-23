#include <chrono>
#include <string>
#include <string_view>
#include <cctype>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/config/MessageConfig.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

class KillGgFeatureSimple final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "killgg"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "KillGG Simple"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "敵を倒すとランダムなGGメッセージをチャット送信します（World Event版）";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) return;

        // World beforeEvents().chatSend にリスナーを登録
        auto& world = api::World::instance();
        mChatListenerId = world.beforeEvents().chatSend.subscribe(
            [this, &mod](api::ChatSendEvent& event) {
                onChatReceive(mod, event.message);
            }
        );

        mEnabled = true;
        mPlayer = std::make_unique<api::Player>(mod);

        mod.getSelf().getLogger().info("KillGG Simple enabled (World Event API)");
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

        mod.getSelf().getLogger().info("KillGG Simple disabled");
    }

private:
    bool mEnabled{false};
    uint64_t mChatListenerId{0};
    std::unique_ptr<api::Player> mPlayer;
    std::chrono::steady_clock::time_point mLastGgTime{};

    void onChatReceive(origin_mod::OriginMod& mod, const std::string& message) {
        if (!mEnabled || !mPlayer) return;

        // MessageConfigから設定を取得
        auto& messageConfig = config::MessageConfig::instance();
        const auto& killggConfig = messageConfig.getKillGGConfig();

        if (!killggConfig.enabled) return;

        // プレイヤー名を取得
        auto playerName = mPlayer->name();
        if (playerName.empty()) return;

        // World APIでキルメッセージを検出
        auto& world = api::World::instance();
        if (!world.isKillMessage(message)) return;

        // メッセージにプレイヤー名が含まれているかチェック
        std::string cleanMessage = world.removeColorCodes(message);
        bool hasPlayerName = (message.find(playerName) != std::string::npos) ||
                           (cleanMessage.find(playerName) != std::string::npos);

        if (!hasPlayerName) return;

        mod.getSelf().getLogger().info("KillGG: Kill detected in message: '{}'", message);

        // 対戦相手の名前を推測
        std::string targetName = extractTargetName(cleanMessage, playerName);
        if (targetName.empty()) {
            targetName = extractTargetNameFromColoredMessage(message, playerName);
        }
        if (targetName.empty()) targetName = "相手";

        // クールダウンチェック
        auto now = std::chrono::steady_clock::now();
        if (now - mLastGgTime < std::chrono::milliseconds(killggConfig.cooldown)) return;

        // MessageConfigからランダムメッセージを取得して送信
        auto ggMessage = messageConfig.getRandomKillGGMessage(targetName);
        if (ggMessage.has_value()) {
            try {
                mPlayer->sendMessage(ggMessage.value());
                mod.getSelf().getLogger().debug("KillGG: Sent message: {}", ggMessage.value());
                mLastGgTime = now;
            } catch (...) {
                mod.getSelf().getLogger().debug("KillGG: Failed to send message");
            }
        }
    }

    std::string extractTargetName(const std::string& message, const std::string& playerName) {
        // 日本語の場合：「（名前）さんを倒しました」パターン
        size_t sanPos = message.find("さんを");
        if (sanPos != std::string::npos) {
            // 「さん」より前の名前部分を抽出
            size_t start = 0;
            for (size_t i = sanPos; i > 0; --i) {
                char c = message[i-1];
                if (c == ' ' || c == '\t' || c == '\n') {
                    start = i;
                    break;
                }
            }
            std::string candidate = message.substr(start, sanPos - start);
            if (!candidate.empty() && candidate != playerName) {
                return candidate;
            }
        }

        // 英語の場合：「killed (名前)」パターン
        size_t killedPos = message.find("killed ");
        if (killedPos != std::string::npos) {
            size_t nameStart = killedPos + 7; // "killed "の長さ
            size_t nameEnd = message.find_first_of(" \t\n", nameStart);
            if (nameEnd == std::string::npos) nameEnd = message.length();

            std::string candidate = message.substr(nameStart, nameEnd - nameStart);
            if (!candidate.empty() && candidate != playerName) {
                return candidate;
            }
        }

        return "相手";
    }

    std::string extractTargetNameFromColoredMessage(const std::string& message, const std::string& playerName) {
        // 色コード付きメッセージから対戦相手名を抽出
        // 今後はWorld APIを通してプレイヤーリストを取得

        // "§9" や "§a" などの色コードに続く名前を探す
        for (size_t i = 0; i < message.length() - 2; ++i) {
            if (message[i] == '§') {
                // 色コードの次の文字をスキップ
                size_t nameStart = i + 2;

                // スペースがあればスキップ
                while (nameStart < message.length() && message[nameStart] == ' ') {
                    ++nameStart;
                }

                // 名前の終端を探す
                size_t nameEnd = nameStart;
                while (nameEnd < message.length()) {
                    char c = message[nameEnd];
                    if (c == '§' || c == ' ' || c == '\t' || c == '\n' ||
                        c == '\r' || nameEnd - nameStart > 20) {
                        break;
                    }
                    ++nameEnd;
                }

                if (nameEnd > nameStart) {
                    std::string candidate = message.substr(nameStart, nameEnd - nameStart);

                    // 基本的な検証
                    if (!candidate.empty() &&
                        candidate != playerName &&
                        candidate != "あなたは" &&
                        candidate != "をキルしました" &&
                        candidate != "が" && candidate != "を" &&
                        candidate.find("キル") == std::string::npos &&
                        candidate.find("倒し") == std::string::npos) {

                        // 英数字を含むかチェック
                        bool hasAlphaNum = false;
                        for (char ch : candidate) {
                            if (std::isalnum(ch)) {
                                hasAlphaNum = true;
                                break;
                            }
                        }

                        if (hasAlphaNum) {
                            return candidate;
                        }
                    }
                }
            }
        }

        return "相手";
    }
};

} // namespace origin_mod::features

namespace {
void registerKillGgSimple(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::KillGgFeatureSimple>("killgg");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerKillGgSimple);
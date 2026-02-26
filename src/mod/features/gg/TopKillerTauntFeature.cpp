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

class TopKillerTauntFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "topkillertaunt"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Top Killer Taunt"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "最多キルプレイヤーが発表されたときに挑発的なメッセージを送信します";
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

        mod.getSelf().getLogger().info("Top Killer Taunt enabled");
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
        mLastTauntTime = {};
        mEnabled = false;

        mod.getSelf().getLogger().info("Top Killer Taunt disabled");
    }

private:
    bool mEnabled{false};
    uint64_t mChatListenerId{0};
    std::unique_ptr<api::Player> mPlayer;
    std::chrono::steady_clock::time_point mLastTauntTime{};

    void onChatReceive(origin_mod::OriginMod& mod, const std::string& message) {
        if (!mEnabled || !mPlayer) return;

        // MessageConfigから設定を取得
        auto& messageConfig = config::MessageConfig::instance();
        const auto& tauntConfig = messageConfig.getTopKillerTauntConfig();

        if (!tauntConfig.enabled) return;

        // 色コードを除去してメッセージを解析
        auto& world = api::World::instance();
        std::string cleanMessage = world.removeColorCodes(message);

        // デバッグ: メッセージの内容を詳しくログ出力
        mod.getSelf().getLogger().debug("Top Killer Taunt: Original message: '{}'", message);
        mod.getSelf().getLogger().debug("Top Killer Taunt: Clean message: '{}'", cleanMessage);

        // 「新しい最多キルプレイヤーです！」パターンを検出
        if (!isTopKillerMessage(cleanMessage)) {
            mod.getSelf().getLogger().debug("Top Killer Taunt: Message pattern not matched");
            return;
        }

        mod.getSelf().getLogger().debug("Top Killer Taunt: Message pattern matched!");

        // トップキラーの名前を抽出
        std::string topKillerName = extractTopKillerName(message, cleanMessage);
        mod.getSelf().getLogger().debug("Top Killer Taunt: Extracted name: '{}'", topKillerName);

        if (topKillerName.empty()) {
            mod.getSelf().getLogger().debug("Top Killer Taunt: Failed to extract top killer name");
            return;
        }

        // 自分の名前かチェック
        auto playerName = mPlayer->name();
        if (playerName.empty() || topKillerName == playerName) return;

        mod.getSelf().getLogger().info("Top Killer Taunt: New top killer detected: '{}'", topKillerName);

        // クールダウンチェック
        auto now = std::chrono::steady_clock::now();
        if (now - mLastTauntTime < std::chrono::milliseconds(tauntConfig.cooldown)) return;

        // MessageConfigからランダム挑発メッセージを取得して送信
        auto tauntMessage = messageConfig.getRandomTopKillerTauntMessage(topKillerName);
        if (tauntMessage.has_value()) {
            try {
                mPlayer->sendMessage(tauntMessage.value());
                mod.getSelf().getLogger().debug("Top Killer Taunt: Sent message: {}", tauntMessage.value());
                mLastTauntTime = now;
            } catch (...) {
                mod.getSelf().getLogger().debug("Top Killer Taunt: Failed to send message");
            }
        }
    }

    bool isTopKillerMessage(const std::string& message) {
        return message.find("が新しい最多キルプレイヤーです！") != std::string::npos ||
               message.find("が新しい最多キルプレイヤーです") != std::string::npos ||
               message.find("新しい最多キルプレイヤーです！") != std::string::npos ||
               message.find("新しい最多キルプレイヤーです") != std::string::npos;
    }

    std::string extractTopKillerName(const std::string& originalMessage, const std::string& cleanMessage) {
        // World APIからプレイヤー名リストを取得
        auto& world = api::World::instance();
        std::vector<std::string> playerNames = world.getPlayerNames();

        // 最初に色コード除去済みメッセージで試行（最も確実）
        size_t patternPos = cleanMessage.find("が新しい最多キルプレイヤーです");
        if (patternPos != std::string::npos) {
            std::string beforePattern = cleanMessage.substr(0, patternPos);

            // 後ろから最初の有効な単語を探す
            std::string extractedName;
            bool foundWordEnd = false;

            for (int i = static_cast<int>(beforePattern.length()) - 1; i >= 0; --i) {
                char c = beforePattern[i];

                if (std::isalnum(c) || c == '_') {
                    extractedName = c + extractedName;
                    foundWordEnd = true;
                } else if (foundWordEnd) {
                    // 名前の区切りに到達
                    break;
                }
            }

            // プレイヤーリストと照合して検証
            if (!extractedName.empty() && extractedName.length() >= 3 && extractedName.length() <= 20) {
                for (const auto& playerName : playerNames) {
                    if (playerName == extractedName) {
                        return extractedName; // 実在するプレイヤー名
                    }
                }
            }
        }

        // フォールバック: オリジナルメッセージから正規表現的にパターンマッチ
        patternPos = originalMessage.find("が新しい最多キルプレイヤーです");
        if (patternPos == std::string::npos) {
            patternPos = originalMessage.find("新しい最多キルプレイヤーです");
        }
        if (patternPos == std::string::npos) return "";

        // パターンの直前から名前を検索
        std::string candidateName;

        // 逆方向で色コードを無視しながら名前を抽出
        for (int i = static_cast<int>(patternPos) - 1; i >= 0; --i) {
            char c = originalMessage[i];

            // 色コードスキップ
            if (c == '§') {
                if (i + 1 < static_cast<int>(originalMessage.length())) {
                    --i; // 次の文字もスキップ
                    continue;
                }
            }

            // スペースなどの区切り文字
            if (c == ' ' || c == '\t' || c == '»' || c == '\r' || c == '\n') {
                if (!candidateName.empty()) {
                    break; // 名前の開始に到達
                }
                continue; // まだ名前を見つけていない
            }

            // 英数字やアンダースコア
            if (std::isalnum(c) || c == '_') {
                candidateName = c + candidateName;
            } else {
                // その他の文字で名前が見つかっている場合は終了
                if (!candidateName.empty()) {
                    break;
                }
            }
        }

        // プレイヤーリストと照合して最終検証
        if (!candidateName.empty() && candidateName.length() >= 3 && candidateName.length() <= 20) {
            for (const auto& playerName : playerNames) {
                if (playerName == candidateName) {
                    return candidateName; // 実在するプレイヤー名
                }
            }
        }

        // どちらの方法でも実在するプレイヤー名が見つからない場合
        // 部分一致でも試行（プレイヤー名の一部が色コードで分割されている場合）
        for (const auto& playerName : playerNames) {
            if (playerName.length() >= 3) {
                // オリジナルメッセージ内にプレイヤー名が含まれているかチェック
                if (originalMessage.find(playerName) != std::string::npos) {
                    // パターンより前にあることを確認
                    size_t namePos = originalMessage.find(playerName);
                    if (namePos < patternPos) {
                        return playerName;
                    }
                }
            }
        }

        return "";
    }
};

} // namespace origin_mod::features

namespace {
void registerTopKillerTaunt(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::TopKillerTauntFeature>("topkillertaunt");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerTopKillerTaunt);
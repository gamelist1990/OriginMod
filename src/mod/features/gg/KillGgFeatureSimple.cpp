#include <array>
#include <chrono>
#include <random>
#include <string>
#include <string_view>
#include <cctype>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

// 20種類のGGメッセージテンプレート
constexpr std::array<std::string_view, 20> kGgTemplates{
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
    "{}さんGG ありがとう！",
};

// キルメッセージを検出するキーワード（色コード対応）
constexpr std::array<std::string_view, 15> kKillKeywords{
    "を倒しました",
    "を倒した",
    "をキルしました",
    "をキルした",
    "をキル",
    "killed",
    "slain",
    "defeated",
    "eliminated",
    "をやっつけました",
    "を撃破",
    "を討伐",
    "died",
    "§7をキルしました", // 色コード付きパターン
    "§7が §", // 汎用的な色コードパターン
};

class KillGgFeatureSimple final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "killgg"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "KillGG Simple"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "敵を倒すとランダムなGGメッセージをチャット送信します（シンプル版）";
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

        mod.getSelf().getLogger().info("KillGG Simple enabled (onReceiveChat mode)");
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

        // プレイヤー名を取得
        auto playerName = mPlayer->name();
        if (playerName.empty()) return;

        // 色コードを除去した版でも確認
        std::string cleanMessage = removeColorCodes(message);

        mod.getSelf().getLogger().debug("KillGG checking message: '{}' (clean: '{}')", message, cleanMessage);

        // メッセージにプレイヤー名が含まれているかチェック（元の色コード付きと除去版両方）
        bool hasPlayerName = (message.find(playerName) != std::string::npos) ||
                           (cleanMessage.find(playerName) != std::string::npos);

        if (!hasPlayerName) return;

        // キルメッセージかどうか判定（色コード除去版で確認）
        bool isKillMessage = false;
        for (const auto& keyword : kKillKeywords) {
            if (message.find(keyword) != std::string::npos ||
                cleanMessage.find(keyword) != std::string::npos) {
                isKillMessage = true;
                mod.getSelf().getLogger().info("KillGG: Kill detected with keyword '{}' in message: '{}'",
                    keyword, message);
                break;
            }
        }

        if (!isKillMessage) return;

        // 対戦相手の名前を推測（色コード除去版で解析）
        std::string targetName = extractTargetName(cleanMessage, playerName);
        if (targetName.empty()) {
            targetName = extractTargetNameFromColoredMessage(message, playerName);
        }
        if (targetName.empty()) return;

        // クールダウンチェック（800ms）
        auto now = std::chrono::steady_clock::now();
        if (now - mLastGgTime < std::chrono::milliseconds(800)) return;

        // ランダムなGGメッセージを送信
        sendRandomGgMessage(mod, targetName);
        mLastGgTime = now;
    }

    std::string extractTargetName(const std::string& message, const std::string& playerName) {
        // 簡単な方法：プレイヤー名以外で「さん」が付く部分を探す
        // もしくは英語名（スペースで区切られた単語）を探す

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

        // 英語の場合：「killed (名前)」パターンを簡単にチェック
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

        // 見つからなければ汎用の「相手」を使用
        return "相手";
    }

    std::string extractTargetNameFromColoredMessage(const std::string& message, const std::string& playerName) {
        // 色コード付きメッセージから対戦相手名を抽出
        // パターン例: "§c§l» §r§7あなたは §9UsualBirch3352 をキルしました"

        // まずプレイヤーリストを取得
        auto& world = api::World::instance();
        std::vector<std::string> playerNames = world.getPlayerNames();

        // "§9" や "§a" などの色コードに続く名前を探す
        for (size_t i = 0; i < message.length() - 2; ++i) {
            if (message[i] == '§') {
                // 色コードの次の文字をスキップ
                size_t nameStart = i + 2;

                // スペースがあればスキップ
                while (nameStart < message.length() && message[nameStart] == ' ') {
                    ++nameStart;
                }

                // 名前の終端を探す（次の色コードまたは特定文字まで）
                size_t nameEnd = nameStart;
                while (nameEnd < message.length()) {
                    char c = message[nameEnd];
                    if (c == '§' || c == ' ' || c == '\t' || c == '\n' ||
                        c == '\r' || nameEnd - nameStart > 20) { // 名前が長すぎる場合は終了
                        break;
                    }
                    ++nameEnd;
                }

                if (nameEnd > nameStart) {
                    std::string candidate = message.substr(nameStart, nameEnd - nameStart);

                    // 空文字、自分の名前、日本語のテキストは除外
                    if (!candidate.empty() &&
                        candidate != playerName &&
                        candidate != "あなたは" &&
                        candidate != "をキルしました" &&
                        candidate != "が" &&
                        candidate != "を" &&
                        candidate.find("キル") == std::string::npos &&
                        candidate.find("倒し") == std::string::npos) {

                        // プレイヤーリストに存在するかチェック
                        bool isValidPlayer = false;
                        for (const auto& knownPlayer : playerNames) {
                            if (candidate == knownPlayer) {
                                isValidPlayer = true;
                                break;
                            }
                        }

                        if (isValidPlayer) {
                            return candidate;
                        }

                        // プレイヤーリストにない場合でも、英数字を含む名前らしいものなら候補として返す
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

    void sendRandomGgMessage(origin_mod::OriginMod& mod, const std::string& targetName) {
        // ランダムなテンプレートを選択
        static thread_local std::mt19937 rng{std::random_device{}()};
        std::uniform_int_distribution<size_t> dist(0, kGgTemplates.size() - 1);

        auto templateStr = kGgTemplates[dist(rng)];
        auto message = fmt::format(fmt::runtime(templateStr), targetName);

        try {
            mPlayer->sendMessage(message);
            mod.getSelf().getLogger().debug("KillGG: Sent message: {}", message);
        } catch (...) {
            mod.getSelf().getLogger().debug("KillGG: Failed to send message");
        }
    }
};

} // namespace origin_mod::features

namespace {
void registerKillGgSimple(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::KillGgFeatureSimple>("killgg");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerKillGgSimple);
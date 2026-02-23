#include "mod/events/GameStateEvent.h"
#include <array>
#include <regex>
#include <optional>

namespace origin_mod::events {

// ゲーム終了を検出するキーワード（AutoGgFeatureから移動）
constexpr std::array<std::string_view, 26> kGameEndKeywords{
    // 日本語パターン
    "ゲーム終了", "試合終了", "勝利", "敗北", "勝ち", "負け", "優勝",
    "ゲームオーバー", "GAME OVER", "Game Over",

    // 英語パターン
    "won the game", "has won the game", "You won", "You lost", "Victory",
    "Defeat", "Winner", "Game End", "Match End", "Champions", "Champion",

    // サーバー固有パターン
    "§a won the game!", "§a has won the game!", "§aYou Win!",
    "§cGame Over!", "Team§r§a won"
};

// キルメッセージを検出するキーワード（KillGgFeatureSimpleから移動）
constexpr std::array<std::string_view, 15> kKillKeywords{
    "を倒しました", "を倒した", "をキルしました", "をキルした", "をキル",
    "killed", "slain", "defeated", "eliminated",
    "をやっつけました", "を撃破", "を討伐", "died",
    "§7をキルしました", "§7が §"
};

std::optional<GameStateEventData> GameStateEventHelper::detectFromChatMessage(const std::string& message) {
    // まずキルメッセージを確認
    auto killEvent = detectKillMessage(message);
    if (killEvent.has_value()) {
        return killEvent;
    }

    // ゲーム終了メッセージを確認
    auto gameEndEvent = detectGameEndMessage(message);
    if (gameEndEvent.has_value()) {
        return gameEndEvent;
    }

    return std::nullopt;
}

std::optional<GameStateEventData> GameStateEventHelper::detectKillMessage(const std::string& message) {
    std::string cleanMessage = removeColorCodes(message);

    // キルキーワードで検出
    for (const auto& keyword : kKillKeywords) {
        if (message.find(keyword) != std::string::npos ||
            cleanMessage.find(keyword) != std::string::npos) {

            return GameStateEventData{
                GameStateEventData::Type::Kill,
                message,
                "", // プレイヤー名は後で抽出可能
                "", // ターゲット名も同様
            };
        }
    }

    return std::nullopt;
}

std::optional<GameStateEventData> GameStateEventHelper::detectGameEndMessage(const std::string& message) {
    std::string cleanMessage = removeColorCodes(message);

    // ゲーム終了キーワードで検出
    for (const auto& keyword : kGameEndKeywords) {
        if (message.find(keyword) != std::string::npos ||
            cleanMessage.find(keyword) != std::string::npos) {

            return GameStateEventData{
                GameStateEventData::Type::GameEnd,
                message,
                "", // 詳細な解析は後で実装可能
            };
        }
    }

    // 正規表現パターンもチェック
    static const std::regex championRegex(R"(Is The §6§l(Chronos|Rush) (Champion|Champions)!)");
    if (std::regex_search(message, championRegex)) {
        return GameStateEventData{
            GameStateEventData::Type::Victory,
            message,
        };
    }

    return std::nullopt;
}

std::string GameStateEventHelper::removeColorCodes(const std::string& text) {
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

} // namespace origin_mod::events
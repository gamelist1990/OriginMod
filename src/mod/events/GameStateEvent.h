#pragma once

#include <string>
#include <optional>

namespace origin_mod::events {

/**
 * ゲーム状態イベント
 * ゲーム開始、終了、勝利、敗北などの状態変化を処理
 */
struct GameStateEventData {
    enum class Type {
        GameStart,      // ゲーム開始
        GameEnd,        // ゲーム終了
        Victory,        // 勝利
        Defeat,         // 敗北
        RoundStart,     // ラウンド開始
        RoundEnd,       // ラウンド終了
        PlayerJoin,     // プレイヤー参加
        PlayerLeave,    // プレイヤー離脱
        Kill,           // キル発生
        Death,          // デス発生
        Custom          // カスタムイベント
    };

    Type type;                  // イベントタイプ
    std::string message;        // 元メッセージ
    std::string playerName;     // 関連プレイヤー名（必要に応じて）
    std::string targetName;     // 対象名（キル/デス等）
    std::string details;        // 追加詳細情報

    GameStateEventData(Type t, std::string msg = "", std::string player = "", std::string target = "")
    : type(t), message(std::move(msg)), playerName(std::move(player)), targetName(std::move(target)) {}
};

/**
 * ゲーム状態イベント検出ヘルパー
 * チャットメッセージからゲーム状態の変化を検出
 */
class GameStateEventHelper {
public:
    /**
     * チャットメッセージからゲーム状態イベントを検出
     * @param message チャットメッセージ
     * @return 検出されたイベント（nulloptの場合は該当なし）
     */
    static std::optional<GameStateEventData> detectFromChatMessage(const std::string& message);

    /**
     * キルメッセージの検出
     */
    static std::optional<GameStateEventData> detectKillMessage(const std::string& message);

    /**
     * ゲーム終了メッセージの検出
     */
    static std::optional<GameStateEventData> detectGameEndMessage(const std::string& message);

private:
    GameStateEventHelper() = default;

    // 色コード除去ユーティリティ
    static std::string removeColorCodes(const std::string& text);
};

} // namespace origin_mod::events
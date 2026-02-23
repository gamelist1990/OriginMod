#include "mod/api/World.h"
#include "mod/OriginMod.h"

// LeviLamina includes for player access
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerListEntry.h"
#include "mc/world/level/dimension/Dimension.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

#include <algorithm>
#include <array>
#include <regex>

namespace origin_mod::api {

// キルメッセージを検出するキーワード
constexpr std::array<std::string_view, 15> kKillKeywords{
    "を倒しました", "を倒した", "をキルしました", "をキルした", "をキル",
    "killed", "slain", "defeated", "eliminated",
    "をやっつけました", "を撃破", "を討伐", "died",
    "§7をキルしました", "§7が §"
};

// ゲーム終了を検出するキーワード
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

World& World::instance() {
    static World g;
    return g;
}

std::vector<std::string> World::getPlayerNames() const {
    std::vector<std::string> playerNames;

    try {
        // Florial方式: clientInstance->getLocalPlayer()->getLevel()
        auto clientInstance = ll::service::bedrock::getClientInstance();
        if (!clientInstance) return playerNames;

        ::LocalPlayer* localPlayer = clientInstance->getLocalPlayer();
        if (!localPlayer) return playerNames;

        ::Level* level = &localPlayer->getLevel();
        if (!level) return playerNames;

        // getPlayerList()を使ってタブリストから取得
        try {
            auto playerMap = level->getPlayerList();
            for (const auto& [uuid, entry] : playerMap) {
                // TypedStorage::get()を使用して値を取得
                try {
                    const std::string& name = entry.mName.get();
                    if (!name.empty()) {
                        // 重複チェック
                        if (std::find(playerNames.begin(), playerNames.end(), name) == playerNames.end()) {
                            playerNames.push_back(name);
                        }
                    }
                } catch (...) {
                    // get()が失敗した場合
                }
            }
        } catch (...) {
            // getPlayerListが失敗した場合
        }

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return playerNames;
}

std::vector<origin_mod::api::Player> World::getPlayers(origin_mod::OriginMod& mod) const {
    std::vector<origin_mod::api::Player> players;

    try {
        // Florial方式: clientInstance->getLocalPlayer()->getLevel()
        auto clientInstance = ll::service::bedrock::getClientInstance();
        if (!clientInstance) return players;

        ::LocalPlayer* localPlayer = clientInstance->getLocalPlayer();
        if (!localPlayer) return players;

        ::Level* level = &localPlayer->getLevel();
        if (!level) return players;

        // getRuntimeActorList()からプレイヤーを収集
        auto actors = level->getRuntimeActorList();
        for (auto* actor : actors) {
            if (actor && actor->isPlayer()) {
                // origin_mod::api::Player ラッパーを作成
                players.emplace_back(mod);
            }
        }

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return players;
}

std::vector<origin_mod::api::Entity> World::getEntities(origin_mod::OriginMod& mod) const {
    std::vector<origin_mod::api::Entity> entities;

    try {
        // Florial方式: clientInstance->getLocalPlayer()->getLevel()
        auto clientInstance = ll::service::bedrock::getClientInstance();
        if (!clientInstance) return entities;

        ::LocalPlayer* localPlayer = clientInstance->getLocalPlayer();
        if (!localPlayer) return entities;

        ::Level* level = &localPlayer->getLevel();
        if (!level) return entities;

        // getRuntimeActorList()を使用してすべてのアクターを取得
        auto actors = level->getRuntimeActorList();
        for (auto* actor : actors) {
            if (actor) {
                entities.emplace_back(actor, mod);
            }
        }

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return entities;
}

// Backward compatibility: 直接イベント発行
void World::onPlayerChat(const std::string& playerName, const std::string& message, origin_mod::OriginMod& mod) {
    ChatSendEvent chatEvent(message, mod);
    emitChatSend(chatEvent);
}

void World::onReceiveChat(const std::string& message, origin_mod::OriginMod& mod) {
    ChatSendEvent chatEvent(message, mod);
    emitChatSend(chatEvent);
}

void World::onPacketReceive(const std::string& packetType, const std::string& message, const std::string& sender, origin_mod::OriginMod& mod) {
    PacketReceiveEvent packetEvent(packetType, message, sender);
    emitPacketReceive(packetEvent);
}

void World::onTick() {
    TickEvent tickEvent;
    emitTick(tickEvent);
}

void World::onItemUse(const std::string& playerName, const std::string& itemName, const std::string& itemType, origin_mod::OriginMod& mod) {
    ItemUseEvent itemEvent(playerName, itemName, itemType);
    emitItemUse(itemEvent);
}

// ゲーム状態検出ヘルパーメソッド
bool World::isKillMessage(const std::string& message) {
    std::string cleanMessage = removeColorCodes(message);

    for (const auto& keyword : kKillKeywords) {
        if (message.find(keyword) != std::string::npos ||
            cleanMessage.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool World::isGameEndMessage(const std::string& message) {
    std::string cleanMessage = removeColorCodes(message);

    // 基本キーワードチェック
    for (const auto& keyword : kGameEndKeywords) {
        if (message.find(keyword) != std::string::npos ||
            cleanMessage.find(keyword) != std::string::npos) {
            return true;
        }
    }

    // 正規表現パターンもチェック
    static const std::regex championRegex(R"(Is The §6§l(Chronos|Rush) (Champion|Champions)!)");
    if (std::regex_search(message, championRegex)) {
        return true;
    }

    return false;
}

std::string World::removeColorCodes(const std::string& text) {
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

} // namespace origin_mod::api
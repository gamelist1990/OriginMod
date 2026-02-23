#include "mod/api/World.h"
#include "mod/OriginMod.h"

// LeviLamina includes for player access
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/actor/player/PlayerListEntry.h"
#include "mc/world/level/dimension/Dimension.h"

#include <algorithm>

namespace origin_mod::api {

World& World::instance() {
    static World g;
    return g;
}

void World::onPlayerChat(const std::string& playerName, const std::string& message, origin_mod::OriginMod& mod) {
    ChatSendEvent chatEvent(message, mod);
    emitChatSend(chatEvent);
}

void World::onReceiveChat(const std::string& message, origin_mod::OriginMod& mod) {
    // 受信したチャットメッセージを処理
    // KillGGなどの機能でこのメッセージを監視できる
    ChatSendEvent chatEvent(message, mod);
    emitChatSend(chatEvent);
}

void World::onPacketReceive(const std::string& packetType, const std::string& message, const std::string& sender, origin_mod::OriginMod& mod) {
    // パケット受信イベントを発行
    PacketReceiveEvent packetEvent(packetType, message, sender);
    emitPacketReceive(packetEvent);
}

void World::onTick() {
    TickEvent tickEvent;
    tickEvent.tick = ++mCurrentTick;
    emitTick(tickEvent);
}

std::vector<std::string> World::getPlayerNames() const {
    std::vector<std::string> playerNames;

    try {
        auto level = ll::service::getLevel();
        if (!level) {
            return playerNames;
        }

        // 方法1: forEachPlayerを使ってプレイヤー名を収集
        level->forEachPlayer(std::function<bool(::Player&)>([&playerNames](::Player& player) -> bool {
            std::string name = player.getRealName();
            if (!name.empty()) {
                playerNames.push_back(name);
            }
            return true; // 継続
        }));

        // 方法2: getPlayerList()を使って追加のプレイヤー情報を収集
        // 注意: PlayerListEntryのメンバーへの直接アクセスが制限されているため一時的に無効化
        /*
        try {
            auto playerList = level->getPlayerList();
            for (const auto& [uuid, entry] : playerList) {
                // 重複チェック
                if (!entry.mName.empty() &&
                    std::find(playerNames.begin(), playerNames.end(), entry.mName) == playerNames.end()) {
                    playerNames.push_back(entry.mName);
                }
            }
        } catch (...) {
            // getPlayerList()が失敗した場合は無視
        }
        */

        // 方法3: getRuntimeActorList()からisPlayer()でフィルタリング（追加分）
        auto actors = level->getRuntimeActorList();
        for (auto* actor : actors) {
            if (actor && actor->isPlayer()) {
                // プレイヤータイプのActorから名前を取得
                std::string name = actor->getNameTag();
                if (name.empty()) {
                    name = actor->getFormattedNameTag();
                }

                // 重複チェック
                if (!name.empty() && std::find(playerNames.begin(), playerNames.end(), name) == playerNames.end()) {
                    playerNames.push_back(name);
                }
            }
        }

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return playerNames;
}

std::vector<origin_mod::api::Player> World::getPlayers(origin_mod::OriginMod& mod) const {
    std::vector<origin_mod::api::Player> players;

    try {
        auto level = ll::service::getLevel();
        if (!level) {
            return players;
        }

        // 方法1: forEachPlayerを使ってPlayer オブジェクトを収集
        level->forEachPlayer(std::function<bool(::Player&)>([&players, &mod](::Player& player) -> bool {
            // origin_mod::api::Player ラッパーを作成
            players.emplace_back(mod);
            return true; // 継続
        }));

        // 方法2: getRuntimeActorList()からisPlayer()でフィルタリング（追加チェック用）
        // 注意: この方法では同じプレイヤーが重複する可能性があるため、
        // 実際のプレイヤー数をカウントする目的で使用

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return players;
}

std::vector<origin_mod::api::Entity> World::getEntities(origin_mod::OriginMod& mod) const {
    std::vector<origin_mod::api::Entity> entities;

    try {
        auto level = ll::service::getLevel();
        if (!level) {
            return entities;
        }

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

} // namespace origin_mod::api

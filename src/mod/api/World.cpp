#include "mod/api/World.h"
#include "mod/OriginMod.h"

// LeviLamina includes for player access
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Player.h"

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

        // forEachPlayerを使ってプレイヤー名を収集
        level->forEachPlayer(std::function<bool(::Player&)>([&playerNames](::Player& player) -> bool {
            std::string name = player.getRealName();
            if (!name.empty()) {
                playerNames.push_back(name);
            }
            return true; // 継続
        }));

    } catch (const std::exception&) {
        // エラー時は空のリストを返す
    }

    return playerNames;
}

} // namespace origin_mod::api

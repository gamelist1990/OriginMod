#include "mod/api/World.h"
#include "mod/OriginMod.h"

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

} // namespace origin_mod::api

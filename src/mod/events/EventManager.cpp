#include "mod/events/EventManager.h"
#include "mod/events/PlayerAttackEvent.h"
#include "mod/events/GameStateEvent.h"
#include "mod/events/ChatEvent.h"
#include "mod/OriginMod.h"

namespace origin_mod::events {

EventManager& EventManager::instance() {
    static EventManager g;
    return g;
}

void EventManager::emitChatSend(ChatSendEvent& ev) {
    mBefore.chatSend.emit(ev);
}

void EventManager::emitPacketReceive(PacketReceiveEvent& ev) {
    mBefore.packetReceive.emit(ev);
}

void EventManager::emitTick(TickEvent& ev) {
    ev.tick = ++mCurrentTick;
    mAfter.tick.emit(ev);
}

void EventManager::emitPlayerAttack(PlayerAttackEventData& ev) {
    mAfter.playerAttack.emit(ev);
}

void EventManager::emitGameState(GameStateEventData& ev) {
    mAfter.gameState.emit(ev);
}

void EventManager::initialize(origin_mod::OriginMod& mod) {
    if (mInitialized) return;

    mCurrentTick = 0;
    mInitialized = true;

    mod.getSelf().getLogger().info("EventManager initialized - centralized event system ready");
}

void EventManager::shutdown() {
    if (!mInitialized) return;

    // 全イベントリスナーのクリア（自動的に行われる）
    mInitialized = false;
}

} // namespace origin_mod::events
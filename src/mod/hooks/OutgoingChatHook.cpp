#include "mod/hooks/OutgoingChatHook.h"

#include <atomic>
#include <sstream>
#include <string>
#include <vector>

#include "ll/api/memory/Hook.h"

#include "mc/network/Packet.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/packet/TextPacket.h"

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/modules/ChatCommandModule.h"

namespace origin_mod::hooks {

namespace {

std::atomic<origin_mod::modules::NativeCommandModule*> gModule{nullptr};
std::atomic<origin_mod::OriginMod*>                        gMod{nullptr};

LL_TYPE_INSTANCE_HOOK(
    OriginModOutgoingChatHook,
    ll::memory::HookPriority::Normal,
    ::LoopbackPacketSender,
    static_cast<void (LoopbackPacketSender::*)(::Packet&)>(&LoopbackPacketSender::$sendToServer),
    void,
    ::Packet& pkt
) {
    auto* self = gModule.load();
    auto* mod  = gMod.load();
    if (!self || !mod) {
        return origin(pkt);
    }

    if (pkt.getId() != ::MinecraftPacketIds::Text) {
        return origin(pkt);
    }

    std::string msg;
    try {
        auto& tp = static_cast<::TextPacket&>(pkt);
        msg = tp.getMessage();
    } catch (...) {
        return origin(pkt);
    }

    if (msg.empty() || msg[0] != '-') {
        return origin(pkt);
    }

    // Emit beforeEvents.chatSend (cancelable)
    {
        origin_mod::api::ChatSendEvent chatEv{msg, *mod};
        origin_mod::api::World::instance().emitChatSend(chatEv);
        if (chatEv.cancel) {
            return; // cancel: do NOT send to server
        }
        msg = chatEv.message;
    }

    // Parse: -<command> [args...]
    std::istringstream iss(msg.substr(1));
    std::vector<std::string> toks;
    std::string tok;
    while (iss >> tok) toks.push_back(tok);

    if (toks.empty()) {
        return; // nothing after hyphen; drop chat
    }

    std::string sub = toks[0];
    std::vector<std::string> args;
    if (toks.size() > 1) args.assign(toks.begin() + 1, toks.end());


    auto reply = [mod](std::string const& s) {
        origin_mod::api::Player{*mod}.localSendMessage(s);
    };

    if (!self->dispatchOriginSubcommand(*mod, sub, args, reply)) {
        origin_mod::api::Player{*mod}.localSendMessage("§cコマンドが見つかりません: {}", sub);
    }

    return; // swallow command chat

    auto reply = [mod](std::string const& s) {
        origin_mod::api::Player{*mod}.localSendMessage(s);
    };

    if (!self->dispatchOriginSubcommand(*mod, sub, args, reply)) {
        origin_mod::api::Player{*mod}.localSendMessage("§cコマンドが見つかりません: {}", sub);
    }

    return; // swallow command chat
}

} // namespace

void installOutgoingChatHook(origin_mod::modules::NativeCommandModule* module, OriginMod* mod) {
    gModule.store(module);
    gMod.store(mod);
    OriginModOutgoingChatHook::hook(); // Re-enabled with LoopbackPacketSender
}

void uninstallOutgoingChatHook() {
    gModule.store(nullptr);
    gMod.store(nullptr);
}

} // namespace origin_mod::hooks

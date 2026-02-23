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

    // Parse: -origin <sub> [args...]
    std::istringstream iss(msg.substr(1));
    std::vector<std::string> toks;
    std::string tok;
    while (iss >> tok) toks.push_back(tok);

    // special-case top-level help shortcut (the user typed "-help" instead of
    // "-origin help").  swallow the chat and execute the same command so the
    // behaviour matches expectations.
    if (toks.size() == 1 && toks[0] == "help") {
        auto reply = [mod](std::string const& s) {
            origin_mod::api::Player{*mod}.localSendMessage(s);
        };
        if (!self->dispatchOriginSubcommand(*mod, "help", {}, reply)) {
            origin_mod::api::Player{*mod}.localSendMessage("§cコマンドが見つかりません: help");
        }
        return; // always swallow
    }

    if (toks.size() < 2) {
        return; // treat as command and swallow
    }
    if (toks[0] != "origin") {
        return origin(pkt); // not ours
    }

    std::string sub = toks[1];
    std::vector<std::string> args;
    if (toks.size() > 2) args.assign(toks.begin() + 2, toks.end());

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

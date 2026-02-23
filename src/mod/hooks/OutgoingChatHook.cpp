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
#include <fmt/format.h>

namespace origin_mod::hooks {

namespace {

// Split a UTF-8 string by common chat separators.
// - ASCII whitespace (space, tab, CR, LF, etc.)
// - Full-width space U+3000 (UTF-8: E3 80 80)
static std::vector<std::string> splitChatTokens(std::string const& s, size_t startIndex) {
    std::vector<std::string> out;
    std::string              cur;

    auto flush = [&]() {
        if (!cur.empty()) {
            out.emplace_back(std::move(cur));
            cur.clear();
        }
    };

    auto isAsciiSpace = [](unsigned char c) {
        switch (c) {
        case ' ': case '\t': case '\n': case '\r': case '\v': case '\f':
            return true;
        default:
            return false;
        }
    };

    static constexpr char kFullWidthSpaceUtf8[] = "\xE3\x80\x80"; // U+3000

    for (size_t i = startIndex; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (isAsciiSpace(c)) {
            flush();
            ++i;
            continue;
        }
        if (i + 2 < s.size() && s.compare(i, 3, kFullWidthSpaceUtf8) == 0) {
            flush();
            i += 3;
            continue;
        }

        cur.push_back(static_cast<char>(c));
        ++i;
    }
    flush();
    return out;
}

static void normalizeSubcommandInPlace(std::string& sub) {
    // Trim ASCII whitespace.
    while (!sub.empty()) {
        unsigned char c = static_cast<unsigned char>(sub.front());
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            sub.erase(sub.begin());
            continue;
        }
        break;
    }
    while (!sub.empty()) {
        unsigned char c = static_cast<unsigned char>(sub.back());
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            sub.pop_back();
            continue;
        }
        break;
    }

    // Trim full-width spaces (U+3000) at both ends.
    static constexpr std::string_view kFw = "\xE3\x80\x80";
    while (sub.size() >= kFw.size() && std::string_view{sub}.starts_with(kFw)) {
        sub.erase(0, kFw.size());
    }
    while (sub.size() >= kFw.size() && std::string_view{sub}.ends_with(kFw)) {
        sub.erase(sub.size() - kFw.size());
    }

    // Be forgiving: allow users to type things like "--help" or "/help".
    while (!sub.empty() && (sub.front() == '-' || sub.front() == '/' || sub.front() == '.')) {
        sub.erase(sub.begin());
    }
}

std::atomic<::origin_mod::modules::NativeCommandModule*> gModule{nullptr};
std::atomic<::origin_mod::OriginMod*>                    gMod{nullptr};

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
    std::vector<std::string> toks = splitChatTokens(msg, /*startIndex*/ 1);

    if (toks.empty()) {
        return; // nothing after hyphen; drop chat
    }

    std::string sub = toks[0];
    normalizeSubcommandInPlace(sub);
    std::vector<std::string> args;
    if (toks.size() > 1) args.assign(toks.begin() + 1, toks.end());


    auto reply = [mod](std::string const& s) {
        origin_mod::api::Player{*mod}.localSendMessage(s);
    };

    (void)self->dispatchOriginSubcommand(*mod, sub, args, reply);

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

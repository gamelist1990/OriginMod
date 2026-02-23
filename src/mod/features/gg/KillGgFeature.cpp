#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>

#include <fmt/format.h>

#include "ll/api/memory/Hook.h"
#include "ll/api/service/Bedrock.h"
#include "ll/api/service/TargetedBedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/PacketSender.h"
#include "mc/network/packet/PlayerListPacket.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/world/Minecraft.h"
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/PlayerListEntry.h"

#include "mod/OriginMod.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace {

using Clock = std::chrono::steady_clock;

// 20 fixed templates. Use one positional placeholder for the target name.
constexpr std::array<std::string_view, 20> kTemplates{
    "{}さんお疲れ～GG",
    "{}さんナイスファイト！GG",
    "{}さん乙！GG",
    "{}さんGG～",
    "{}さんおつでした！",
    "{}さんありがと～GG",
    "{}さんお疲れ様！GG",
    "{}さんGood Game!",
    "{}さんGGWP!",
    "{}さんドンマイ！GG",
    "{}さんお見事！GG",
    "{}さん対戦ありがとうございました！",
    "{}さんまたよろしく～GG",
    "{}さんおつGG",
    "{}さんファイト！GG",
    "{}さんお疲れっした！GG",
    "{}さん(｀・ω・´)b GG",
    "{}さんいい勝負だった！GG",
    "{}さんおつかれ～",
    "{}さんGG ありがとう！",
};

// Remove Minecraft formatting codes (e.g. "§a", "§r").
[[nodiscard]] std::string stripMcFormatting(std::string_view in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '\xC2') {
            // Sometimes UTF-8 section sign may be encoded as 0xC2 0xA7.
            if (i + 1 < in.size() && static_cast<unsigned char>(in[i + 1]) == 0xA7) {
                // Skip "§" and the next color code if exists.
                i += 1;
                if (i + 1 < in.size()) {
                    i += 1;
                }
                continue;
            }
        }
        if (c == '\xA7' || c == '§') {
            // Skip section sign and next code char if exists.
            if (i + 1 < in.size()) {
                i += 1;
            }
            continue;
        }
        out.push_back(c);
    }
    return out;
}

[[nodiscard]] bool isNameChar(unsigned char c) {
    return (std::isalnum(c) != 0) || c == '_';
}

[[nodiscard]] bool isBoundaryMatch(std::string const& s, size_t pos, size_t len) {
    if (pos == std::string::npos) {
        return false;
    }
    bool leftOk = (pos == 0) || !isNameChar(static_cast<unsigned char>(s[pos - 1]));
    bool rightOk = (pos + len >= s.size()) || !isNameChar(static_cast<unsigned char>(s[pos + len]));
    return leftOk && rightOk;
}

// Simple token lists to recognize "you-killed-<player>" style messages (Japanese/English).
static constexpr std::array<std::string_view, 6> kPositiveVerbsJa{
    "を倒しました",
    "を倒した",
    "をキルしました",
    "をキルした",
    "との戦いを制しました",
    "を撃破しました",
};
static constexpr std::array<std::string_view, 4> kPositiveVerbsEn{
    "killed",
    "slain",
    "defeated",
    "eliminated",
};
// Negative verbs that mean "you were killed" (we must NOT trigger GG for these).
static constexpr std::array<std::string_view, 4> kNegativeVerbsJa{
    "にキルされました",
    "に倒されました",
    "に敗れました",
    "にやられました",
};
static constexpr std::array<std::string_view, 3> kPronouns{
    "あなた",
    "You",
    "you",
};

static bool containsAny(std::string_view hay, std::string_view needle) {
    return hay.find(needle) != std::string_view::npos;
}

static bool containsAny(std::string_view hay, std::initializer_list<std::string_view> needles) {
    for (auto n : needles) {
        if (hay.find(n) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}

// Generic container overload (std::array, vector, etc.).
template <typename Container>
static bool containsAnyOf(std::string_view hay, Container const& needles) {
    for (auto const& n : needles) {
        if (hay.find(n) != std::string_view::npos) {
            return true;
        }
    }
    return false;
}


[[nodiscard]] std::string actorDisplayName(::Actor& a) {
    // Prefer name tag (player name / custom name), fallback to type name.
    std::string name;
    try {
        name = a.getFormattedNameTag();
    } catch (...) {
        // Some symbols may be unavailable depending on platform/version.
    }
    if (name.empty()) {
        try {
            name = a.getTypeName();
        } catch (...) {
        }
    }
    if (name.empty()) {
        name = "敵";
    }
    return name;
}

[[nodiscard]] bool sendChatToServer(std::string const& author, std::string const& msg) {
    auto mcOpt = ll::service::bedrock::getMinecraft(true);
    if (!mcOpt) {
        return false;
    }
    auto& mc = *mcOpt;

    // Client->Server chat.
    // xuid/platformId are left empty; we only provide author for better compatibility/formatting.
    auto pkt = ::TextPacketPayload::createChat(author, msg, std::nullopt, "", "");
    mc.mPacketSender.sendToServer(pkt);
    return true;
}

template <class URBG>
[[nodiscard]] std::string pickMessage(URBG& rng, std::string_view targetName) {
    std::uniform_int_distribution<size_t> dist(0, kTemplates.size() - 1);
    auto tpl = kTemplates[dist(rng)];
    return fmt::format(fmt::runtime(tpl), targetName);
}

} // namespace

namespace origin_mod::features {

class KillGgFeature;

} // namespace origin_mod::features

namespace {

void dispatchKillGgTextPacket(::TextPacket const& pkt);
void dispatchKillGgPlayerListPacket(::PlayerListPacket const& pkt);

// Active instance pointer for hooks.
std::atomic<origin_mod::features::KillGgFeature*> gKillGgActive{nullptr};
std::atomic<origin_mod::OriginMod*>                  gKillGgMod{nullptr};

} // namespace

namespace origin_mod::features {

// ---------------------------------------------------------------------------
// Hooks: packet deserialization (incoming)
// ---------------------------------------------------------------------------

LL_TYPE_INSTANCE_HOOK(
    KillGgTextPacketReadHook,
    ll::memory::HookPriority::Normal,
    TextPacket,
    static_cast<::Bedrock::Result<void> (TextPacket::*)(::ReadOnlyBinaryStream&)>(&TextPacket::$_read),
    ::Bedrock::Result<void>,
    ::ReadOnlyBinaryStream& stream
) {
    auto res = origin(stream);
    if (res) {
        dispatchKillGgTextPacket(*this);
    }
    return res;
}

LL_TYPE_INSTANCE_HOOK(
    KillGgPlayerListPacketReadHook,
    ll::memory::HookPriority::Normal,
    PlayerListPacket,
    static_cast<::Bedrock::Result<void> (PlayerListPacket::*)(::ReadOnlyBinaryStream&)>(&PlayerListPacket::$_read),
    ::Bedrock::Result<void>,
    ::ReadOnlyBinaryStream& stream
) {
    auto res = origin(stream);
    if (res) {
        dispatchKillGgPlayerListPacket(*this);
    }
    return res;
}

class KillGgFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "killgg"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "KillGG"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "敵を倒すとランダムなGGメッセージをチャット送信します";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) {
            return;
        }

        // Install hooks once.
        if (!mHooked) {
            KillGgTextPacketReadHook::hook();
            KillGgPlayerListPacketReadHook::hook();
            mHooked = true;
            mod.getSelf().getLogger().debug("KillGG: packet read hooks installed");
        }

        // Try to resolve local player name early (may fail until in-game).
        (void)refreshLocalName();

        gKillGgMod.store(&mod, std::memory_order_relaxed);
        gKillGgActive.store(this, std::memory_order_relaxed);

        mEnabled = true;
        mod.getSelf().getLogger().debug("KillGG enabled");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        if (!mEnabled) {
            return;
        }

        gKillGgActive.store(nullptr, std::memory_order_relaxed);
        gKillGgMod.store(nullptr, std::memory_order_relaxed);

        mLastSentAt.reset();
        {
            std::scoped_lock lk(mMutex);
            mLocalName.clear();
            mPlayerNames.clear();
            mLastMatchedMessage.clear();
        }
        mEnabled = false;

        mod.getSelf().getLogger().debug("KillGG disabled");
    }

    // Called from hooks (after packet is handled by vanilla).
    void onIncomingPlayerListPacket(origin_mod::OriginMod&, ::PlayerListPacket const& pkt) {
        // Update player name list for kill message auto-detection.
        auto const& entries = static_cast<std::vector<::PlayerListEntry> const&>(pkt.mEntries);
        auto action         = static_cast<::PlayerListPacketType const&>(pkt.mAction);

        std::scoped_lock lk(mMutex);
        if (action == ::PlayerListPacketType::Add) {
            for (auto const& e : entries) {
                auto const& name = static_cast<std::string const&>(e.mName);
                if (!name.empty()) {
                    mPlayerNames.insert(name);
                }
            }
        } else if (action == ::PlayerListPacketType::Remove) {
            for (auto const& e : entries) {
                auto const& name = static_cast<std::string const&>(e.mName);
                if (!name.empty()) {
                    mPlayerNames.erase(name);
                }
            }
        }
    }

    // Called from hooks (after packet is handled by vanilla).
    void onIncomingTextPacket(origin_mod::OriginMod& mod, ::TextPacket const& pkt) {
        // Ensure local player name is known.
        if (!ensureLocalNameReady()) {
            return;
        }

        // Extract message text.
        // TextPacket is a PayloadPacket<TextPacketPayload>; TextPacketPayload::getMessage() is expected to be available.
        std::string msg;
        try {
            msg = pkt.getMessage();
        } catch (...) {
            return;
        }
        if (msg.empty()) {
            return;
        }

        auto normalized = stripMcFormatting(msg);
        if (normalized.empty()) {
            return;
        }

        // Must contain self name near the beginning.
        std::string localNameCopy;
        {
            std::scoped_lock lk(mMutex);
            localNameCopy = mLocalName;
        }
        if (localNameCopy.empty()) {
            return;
        }

        auto posSelf = normalized.find(localNameCopy);
        bool selfAppears = (posSelf != std::string::npos && posSelf < 32 && isBoundaryMatch(normalized, posSelf, localNameCopy.size()));

        // If local name not explicitly present, support localized pronoun messages (e.g. "あなたが X を倒しました", "You killed X").
        bool pronounUsed = false;
        size_t posPronoun = std::string::npos;
        if (!selfAppears) {
            for (auto p : kPronouns) {
                auto pos = normalized.find(p);
                if (pos != std::string::npos && pos < 32) {
                    pronounUsed = true;
                    posPronoun = pos;
                    break;
                }
            }
            if (pronounUsed) {
                // avoid cases like "あなたは X にキルされました" (you were killed) by checking for positive verbs only.
                if (!containsAnyOf(normalized, kPositiveVerbsJa) && !containsAnyOf(normalized, kPositiveVerbsEn)) {
                    return; // not a "you killed someone" message
                }
                // ensure there is a known player name AFTER the pronoun
                std::scoped_lock lk(mMutex);
                for (auto const& name : mPlayerNames) {
                    if (name.empty()) {
                        continue;
                    }
                    auto pos = normalized.find(name, posPronoun + 1);
                    if (pos != std::string::npos && isBoundaryMatch(normalized, pos, name.size())) {
                        // treat pronoun position as posSelf so subsequent logic finds the target correctly
                        posSelf = posPronoun;
                        selfAppears = true; // allow flow to continue
                        break;
                    }
                }
                if (!selfAppears) {
                    return;
                }
            }
        }
        if (!selfAppears) {
            return;
        }

        // Avoid double-processing the exact same line.
        {
            std::scoped_lock lk(mMutex);
            if (normalized == mLastMatchedMessage) {
                return;
            }
        }

        // Find a player name that appears after self.
        std::string target;
        size_t      bestPos = std::string::npos;
        {
            std::scoped_lock lk(mMutex);
            for (auto const& name : mPlayerNames) {
                if (name.empty() || name == localNameCopy) {
                    continue;
                }
                auto pos = normalized.find(name, posSelf + localNameCopy.size());
                if (pos == std::string::npos) {
                    continue;
                }
                if (!isBoundaryMatch(normalized, pos, name.size())) {
                    continue;
                }
                if (bestPos == std::string::npos || pos < bestPos) {
                    bestPos = pos;
                    target  = name;
                }
            }
        }
        if (target.empty()) {
            return;
        }

        // Cooldown to avoid chat spam.
        constexpr auto kCooldown = std::chrono::milliseconds(800);
        auto now = Clock::now();
        if (mLastSentAt.has_value() && (now - *mLastSentAt) < kCooldown) {
            return;
        }

        thread_local std::mt19937 rng{std::random_device{}()};
        auto message = pickMessage(rng, target);

        if (!sendChatToServer(localNameCopy, message)) {
            mod.getSelf().getLogger().debug("KillGG: failed to send chat (minecraft not ready?)");
            return;
        }

        {
            std::scoped_lock lk(mMutex);
            mLastMatchedMessage = normalized;
        }
        mLastSentAt = now;
    }

private:
    bool mEnabled{false};

    bool mHooked{false};
    std::optional<Clock::time_point> mLastSentAt;

    mutable std::mutex mMutex;
    std::string                     mLocalName;
    std::optional<Clock::time_point> mLastNameRefreshAt;
    std::unordered_set<std::string> mPlayerNames;
    std::string                     mLastMatchedMessage;

    [[nodiscard]] bool refreshLocalName() {
        // Throttle a bit (network packets may be frequent).
        auto now = Clock::now();
        {
            std::scoped_lock lk(mMutex);
            if (mLastNameRefreshAt.has_value() && (now - *mLastNameRefreshAt) < std::chrono::seconds(1)) {
                return !mLocalName.empty();
            }
            mLastNameRefreshAt = now;
        }

        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (!ciOpt) {
            return false;
        }

        auto* lp = ciOpt->getLocalPlayer();
        if (!lp) {
            return false;
        }

        std::string name;
        try {
            name = lp->getRealName();
        } catch (...) {
        }
        if (name.empty()) {
            // Fallback to nametag if needed.
            try {
                name = actorDisplayName(*lp);
            } catch (...) {
            }
        }
        if (name.empty()) {
            return false;
        }

        std::scoped_lock lk(mMutex);
        mLocalName = std::move(name);
        return true;
    }

    [[nodiscard]] bool ensureLocalNameReady() {
        {
            std::scoped_lock lk(mMutex);
            if (!mLocalName.empty()) {
                return true;
            }
        }
        return refreshLocalName();
    }
};

} // namespace origin_mod::features

namespace {

void dispatchKillGgTextPacket(::TextPacket const& pkt) {
    auto* self = gKillGgActive.load(std::memory_order_relaxed);
    auto* mod  = gKillGgMod.load(std::memory_order_relaxed);
    if (!self || !mod) {
        return;
    }
    self->onIncomingTextPacket(*mod, pkt);
}

void dispatchKillGgPlayerListPacket(::PlayerListPacket const& pkt) {
    auto* self = gKillGgActive.load(std::memory_order_relaxed);
    auto* mod  = gKillGgMod.load(std::memory_order_relaxed);
    if (!self || !mod) {
        return;
    }
    self->onIncomingPlayerListPacket(*mod, pkt);
}

} // namespace

namespace {
void registerKillGg(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::KillGgFeature>("killgg");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerKillGg);

#include "mod/api/Player.h"
#include "mod/api/World.h"

#include "mod/OriginMod.h"

#include "ll/api/service/Bedrock.h"

#include <optional>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/PacketSender.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/world/Minecraft.h"
#include "mc/client/gui/GuiData.h"
#include "mc/world/actor/ActorSwingSource.h"

namespace origin_mod::api {

Player::Player(origin_mod::OriginMod& mod)
: mMod(mod) {}

std::string Player::name() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) {
        return {};
    }

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) {
        return {};
    }

    std::string n;
    try {
        n = lp->getRealName();
    } catch (...) {
    }
    if (!n.empty()) {
        return n;
    }
    try {
        n = lp->getFormattedNameTag();
    } catch (...) {
    }
    return n;
}

void Player::setSneaking(bool sneaking) {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return;

    try {
        lp->setSneaking(sneaking);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to set sneaking state");
    }
}

void Player::setSprinting(bool sprinting) {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return;

    try {
        lp->setSprinting(sprinting);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to set sprinting state");
    }
}

bool Player::isAutoJumpEnabled() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return false;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return false;

    try {
        return lp->isAutoJumpEnabled();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get auto jump state");
        return false;
    }
}

bool Player::swing() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return false;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return false;

    try {
        // Interact swingを使用
        return lp->swing(ActorSwingSource::Interact);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to swing");
        return false;
    }
}

void Player::playEmote(const std::string& emoteId) const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return;

    try {
        lp->playEmote(emoteId, true); // true for chat message
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to play emote: {}", emoteId);
    }
}

bool Player::isTeacher() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return false;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return false;

    try {
        return lp->isTeacher();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get teacher state");
        return false;
    }
}

void Player::sendMessage(std::string_view msg) const {
    // Server-visible chat.
    // Emit `World::ChatSendEvent` to allow listeners to inspect/cancel outgoing chat.
    try {
        origin_mod::api::ChatSendEvent ev(std::string{msg}, mMod);
        World::instance().emitChatSend(ev);
        if (ev.cancel) {
            mMod.getSelf().getLogger().debug("ChatSendEvent cancelled by listener, dropping outgoing chat: {}", msg);
            return;
        }
    } catch (...) {
        // If event system isn't available for some reason, fall back to normal behavior.
    }

    auto mcOpt = ll::service::bedrock::getMinecraft(true);
    if (!mcOpt) {
        // fallback
        localSendMessage(msg);
        return;
    }
    auto& mc = *mcOpt;

    auto author = name();
    if (author.empty()) {
        author = mMod.getSelf().getName();
    }

    auto pkt = ::TextPacketPayload::createChat(author, std::string{msg}, std::nullopt, "", "");
    mc.mPacketSender.sendToServer(pkt);
}

void Player::localSendMessage(std::string_view msg) const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (ciOpt) {
        try {
            if (auto *lp = ciOpt->getLocalPlayer()) {
                lp->displayClientMessage(std::string{msg}, std::nullopt);
                return;
            }
        } catch (...) {
            // エラー時は何もしない
        }
    }

    // Fallback: log so the player can still see something in the console.
    auto &logger = mMod.getSelf().getLogger();
    logger.warn("localSendMessage failed, dropping message: {}", msg);
}

Player::Location Player::location() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return Location();

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return Location();

    try {
        auto pos = lp->getPosition();
        return Location(pos.x, pos.y, pos.z);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get player location");
        return Location();
    }
}

int Player::getHealth() const {
    auto ciOpt = ll::service::bedrock::getClientInstance();
    if (!ciOpt) return -1;

    auto* lp = ciOpt->getLocalPlayer();
    if (!lp) return -1;

    try {
        // Actor::getHealth() 
        return lp->getHealth();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get player health");
        return -1;
    }
}

} // namespace origin_mod::api

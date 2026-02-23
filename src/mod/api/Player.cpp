#include "mod/api/Player.h"

#include "mod/OriginMod.h"
#include "mod/util/DebugLogger.h"

#include "ll/api/service/Bedrock.h"

#include <optional>

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/network/PacketSender.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/world/Minecraft.h"
#include "mc/client/gui/GuiData.h"

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

void Player::sendMessage(std::string_view msg) const {
    // Server-visible chat.
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
            origin_mod::util::debugLog("exception while calling getLocalPlayer/displayClientMessage");
        }
    }

    // Fallback: log so the player can still see something in the console.
    auto &logger = mMod.getSelf().getLogger();
    logger.warn("localSendMessage failed, dropping message: {}", msg);
}

} // namespace origin_mod::api

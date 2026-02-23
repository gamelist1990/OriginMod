#pragma once

#include <string>
#include "mod/api/Player.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::events {

/**
 * チャット送信イベント
 */
struct ChatSendEvent {
    std::string message;
    origin_mod::api::Player sender;
    bool cancel{false};

    ChatSendEvent(std::string msg, origin_mod::OriginMod& mod)
    : message(std::move(msg)),
      sender(mod) {}
};

/**
 * パケット受信イベント
 */
struct PacketReceiveEvent {
    std::string packetType;
    std::string message;
    std::string senderName;
    bool cancel{false};

    PacketReceiveEvent(std::string type, std::string msg, std::string sender = "")
    : packetType(std::move(type)),
      message(std::move(msg)),
      senderName(std::move(sender)) {}
};

/**
 * ティックイベント
 */
struct TickEvent {
    uint64_t tick{0};
};

} // namespace origin_mod::events
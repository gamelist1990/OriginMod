#pragma once

#include <string>
#include <string_view>
#include <utility>

#include <fmt/format.h>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::api {

// Developer-friendly wrapper inspired by Bukkit's Player API.
// This is a lightweight abstraction for "who to reply to".
//
// Current implementation is chat-command oriented: sendMessage() forwards to a sink.
// Later we can extend this to actually push messages into the in-game chat HUD.
class Player {
public:
    explicit Player(origin_mod::OriginMod& mod);

    [[nodiscard]] origin_mod::OriginMod& mod() const noexcept { return mMod; }

    // Player name (best-effort). Returns empty if not in-game yet.
    [[nodiscard]] std::string name() const;

    // Send message to server chat (other users may see it).
    void sendMessage(std::string_view msg) const;

    // fmt-format overload
    template <class... Args>
    void sendMessage(fmt::format_string<Args...> fmtStr, Args&&... args) const {
        sendMessage(fmt::format(fmtStr, std::forward<Args>(args)...));
    }

    // Local-only message (only you see it).
    void localSendMessage(std::string_view msg) const;

    // fmt-format overload
    template <class... Args>
    void localSendMessage(fmt::format_string<Args...> fmtStr, Args&&... args) const {
        localSendMessage(fmt::format(fmtStr, std::forward<Args>(args)...));
    }

private:
    origin_mod::OriginMod& mMod;
};

} // namespace origin_mod::api

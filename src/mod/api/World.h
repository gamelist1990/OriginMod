#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mod/api/Player.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::api {

// Very small signal/slot utility for event subscriptions.
template <class EventT>
class EventSignal {
public:
    using Handler = std::function<void(EventT&)>;

    // Returns subscription id.
    uint64_t subscribe(Handler h) {
        if (!h) return 0;
        std::scoped_lock lk(mMutex);
        uint64_t id = ++mNextId;
        mHandlers.emplace(id, std::move(h));
        return id;
    }

    void unsubscribe(uint64_t id) {
        if (!id) return;
        std::scoped_lock lk(mMutex);
        mHandlers.erase(id);
    }

    void emit(EventT& ev) {
        // Copy handlers to avoid re-entrancy / modification issues.
        std::vector<Handler> hs;
        {
            std::scoped_lock lk(mMutex);
            hs.reserve(mHandlers.size());
            for (auto const& [_, h] : mHandlers) {
                hs.push_back(h);
            }
        }
        for (auto& h : hs) {
            if (h) h(ev);
        }
    }

private:
    std::mutex                         mMutex;
    uint64_t                           mNextId{0};
    std::unordered_map<uint64_t, Handler> mHandlers;
};

struct ChatSendEvent {
    std::string message;
    origin_mod::api::Player sender;
    bool cancel{false};

    ChatSendEvent(std::string msg, origin_mod::OriginMod& mod)
    : message(std::move(msg)),
      sender(mod) {}
};

struct TickEvent {
    uint64_t tick{0};
};

class World {
public:
    static World& instance();

    struct BeforeEvents {
        EventSignal<ChatSendEvent> chatSend;
    };

    struct AfterEvents {
        EventSignal<TickEvent> tick;
    };

    [[nodiscard]] BeforeEvents& beforeEvents() noexcept { return mBefore; }
    [[nodiscard]] AfterEvents& afterEvents() noexcept { return mAfter; }

    // Internal: emitters used by modules.
    void emitChatSend(ChatSendEvent& ev) { mBefore.chatSend.emit(ev); }
    void emitTick(TickEvent& ev) { mAfter.tick.emit(ev); }

    // Public methods called by ChatHook
    void onPlayerChat(const std::string& playerName, const std::string& message, origin_mod::OriginMod& mod);
    void onReceiveChat(const std::string& message, origin_mod::OriginMod& mod);
    void onTick();

private:
    World() = default;

    BeforeEvents mBefore;
    AfterEvents  mAfter;
    uint64_t     mCurrentTick{0};
};

} // namespace origin_mod::api

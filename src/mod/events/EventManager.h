#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::events {

// 汎用イベントシグナル（World.hから移動）
template <class EventT>
class EventSignal {
public:
    using Handler = std::function<void(EventT&)>;

    // 購読ID返却
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
        // 再入・変更問題を避けるためハンドラーをコピー
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

// 前方宣言
struct PlayerAttackEventData;
struct GameStateEventData;
struct ChatSendEvent;
struct TickEvent;
struct PacketReceiveEvent;

/**
 * 統一イベントマネージャー
 * 全てのゲームイベントを一元管理
 */
class EventManager {
public:
    static EventManager& instance();

    // イベント構造体の定義
    struct BeforeEvents {
        EventSignal<ChatSendEvent> chatSend;
        EventSignal<PacketReceiveEvent> packetReceive;
    };

    struct AfterEvents {
        EventSignal<TickEvent> tick;
        EventSignal<PlayerAttackEventData> playerAttack;
        EventSignal<GameStateEventData> gameState;
    };

    [[nodiscard]] BeforeEvents& beforeEvents() noexcept { return mBefore; }
    [[nodiscard]] AfterEvents& afterEvents() noexcept { return mAfter; }

    // イベント発行メソッド
    void emitChatSend(ChatSendEvent& ev);
    void emitPacketReceive(PacketReceiveEvent& ev);
    void emitTick(TickEvent& ev);
    void emitPlayerAttack(PlayerAttackEventData& ev);
    void emitGameState(GameStateEventData& ev);

    // 初期化・終了処理
    void initialize(origin_mod::OriginMod& mod);
    void shutdown();

    [[nodiscard]] bool isInitialized() const noexcept { return mInitialized; }

private:
    EventManager() = default;

    BeforeEvents mBefore;
    AfterEvents  mAfter;
    bool         mInitialized{false};
    uint64_t     mCurrentTick{0};
};

} // namespace origin_mod::events
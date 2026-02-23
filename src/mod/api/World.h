#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mod/api/Player.h"
#include "mod/api/Entity.h"

// LeviLamina includes for Player access
#include "ll/api/service/Bedrock.h"
#include "mc/world/level/Level.h"
#include "mc/world/actor/player/Player.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::api {

// Event signal utility (EventManagerから移動)
template <class EventT>
class EventSignal {
public:
    using Handler = std::function<void(EventT&)>;

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
    std::mutex mMutex;
    uint64_t mNextId{0};
    std::unordered_map<uint64_t, Handler> mHandlers;
};

// イベント構造体の定義
struct ChatSendEvent {
    std::string message;
    origin_mod::api::Player sender;
    bool cancel{false};

    ChatSendEvent(std::string msg, origin_mod::OriginMod& mod)
    : message(std::move(msg)), sender(mod) {}
};

struct ItemUseEvent {
    std::string playerName;
    std::string itemName;
    std::string itemType;
    int itemId{0};
    int itemCount{1};
    bool cancel{false};

    // 使用位置情報
    double posX{0.0}, posY{0.0}, posZ{0.0};

    ItemUseEvent(std::string player, std::string item, std::string type = "")
    : playerName(std::move(player)), itemName(std::move(item)), itemType(std::move(type)) {}
};

struct TickEvent {
    uint64_t tick{0};
};

struct PlayerAttackEventData {
    std::string attackerName;
    std::string targetName;
    std::string targetType;
    double damage;
    bool wasCritical;
    double distance;
    bool targetIsPlayer;

    // HP情報
    float targetCurrentHP{-1.0f};
    float targetMaxHP{-1.0f};

    // 位置情報
    double attackerX, attackerY, attackerZ;
    double targetX, targetY, targetZ;
};

struct GameStateEventData {
    enum class Type {
        GameStart, GameEnd, Victory, Defeat,
        RoundStart, RoundEnd, PlayerJoin, PlayerLeave,
        Kill, Death, Custom
    };

    Type type;
    std::string message;
    std::string playerName;
    std::string targetName;
    std::string details;

    GameStateEventData(Type t, std::string msg = "", std::string player = "", std::string target = "")
    : type(t), message(std::move(msg)), playerName(std::move(player)), targetName(std::move(target)) {}
};

struct PacketReceiveEvent {
    std::string packetType;
    std::string message;
    std::string senderName;
    bool cancel{false};

    PacketReceiveEvent(std::string type, std::string msg, std::string sender = "")
    : packetType(std::move(type)), message(std::move(msg)), senderName(std::move(sender)) {}
};

/**
 * World API - ワールド状態アクセス + 統合イベントシステム
 */
class World {
public:
    static World& instance();

    // イベント構造体
    struct BeforeEvents {
        EventSignal<ChatSendEvent> chatSend;
        EventSignal<PacketReceiveEvent> packetReceive;
        EventSignal<ItemUseEvent> itemUse;
    };

    struct AfterEvents {
        EventSignal<TickEvent> tick;
        EventSignal<PlayerAttackEventData> playerAttack;
        EventSignal<GameStateEventData> gameState;
    };

    // イベントアクセス (従来のMinecraft風)
    [[nodiscard]] BeforeEvents& beforeEvents() noexcept { return mBefore; }
    [[nodiscard]] AfterEvents& afterEvents() noexcept { return mAfter; }

    // イベント発行メソッド (内部使用)
    void emitChatSend(ChatSendEvent& ev) { mBefore.chatSend.emit(ev); }
    void emitPacketReceive(PacketReceiveEvent& ev) { mBefore.packetReceive.emit(ev); }
    void emitItemUse(ItemUseEvent& ev) { mBefore.itemUse.emit(ev); }
    void emitTick(TickEvent& ev) { ev.tick = ++mCurrentTick; mAfter.tick.emit(ev); }
    void emitPlayerAttack(PlayerAttackEventData& ev) { mAfter.playerAttack.emit(ev); }
    void emitGameState(GameStateEventData& ev) { mAfter.gameState.emit(ev); }

    /**
     * プレイヤー名の一覧を取得
     */
    std::vector<std::string> getPlayerNames() const;

    /**
     * プレイヤーオブジェクトの一覧を取得
     */
    std::vector<origin_mod::api::Player> getPlayers(origin_mod::OriginMod& mod) const;

    /**
     * 全エンティティオブジェクトの一覧を取得
     */
    std::vector<origin_mod::api::Entity> getEntities(origin_mod::OriginMod& mod) const;

    /**
     * レベルにアクセスしてプレイヤーを反復処理
     */
    template<typename Func>
    void forEachPlayer(Func&& func) const {
        try {
            // Florial方式: clientInstance->getLocalPlayer()->getLevel()
            auto clientInstance = ll::service::bedrock::getClientInstance();
            if (!clientInstance) return;

            ::LocalPlayer* localPlayer = clientInstance->getLocalPlayer();
            if (!localPlayer) return;

            ::Level* level = &localPlayer->getLevel();
            if (!level) return;

            // RuntimeActorListから取得
            auto actors = level->getRuntimeActorList();
            for (auto* actor : actors) {
                if (actor && actor->isPlayer()) {
                    auto* player = static_cast<::Player*>(actor);
                    func(*player);
                }
            }

        } catch (const std::exception&) {
            // エラー時は何もしない
        }
    }

    // ゲーム状態検出ヘルパーメソッド
    static bool isKillMessage(const std::string& message);
    static bool isGameEndMessage(const std::string& message);
    static std::string removeColorCodes(const std::string& text);

    // Backward compatibility
    void onPlayerChat(const std::string& playerName, const std::string& message, origin_mod::OriginMod& mod);
    void onReceiveChat(const std::string& message, origin_mod::OriginMod& mod);
    void onPacketReceive(const std::string& packetType, const std::string& message, const std::string& sender, origin_mod::OriginMod& mod);
    void onTick();
    void onItemUse(const std::string& playerName, const std::string& itemName, const std::string& itemType, origin_mod::OriginMod& mod);

private:
    World() = default;

    BeforeEvents mBefore;
    AfterEvents mAfter;
    uint64_t mCurrentTick{0};
};

} // namespace origin_mod::api
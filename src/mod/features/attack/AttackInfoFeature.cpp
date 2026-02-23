#include <chrono>
#include <string>
#include <cmath>

#include <fmt/format.h>

#include "mod/OriginMod.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

// LeviLamina includes for attack detection
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/player/PlayerAttackEvent.h"

// MineCraft includes for entity access
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"
#include "mc/world/attribute/AttributeInstance.h"

namespace origin_mod::features {

class AttackInfoFeature final : public IFeature {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "attackinfo"; }
    [[nodiscard]] std::string_view name() const noexcept override { return "Attack Info"; }
    [[nodiscard]] std::string_view description() const noexcept override {
        return "攻撃時に敵の名前とHP情報を表示します";
    }

    void onEnable(origin_mod::OriginMod& mod) override {
        if (mEnabled) return;

        // PlayerAttackEventにリスナーを登録
        auto& eventBus = ll::event::EventBus::getInstance();

        // プレイヤーが攻撃した時のイベント
        mAttackListenerId = eventBus.emplaceListener<ll::event::PlayerAttackEvent>(
            [this, &mod](ll::event::PlayerAttackEvent& event) {
                onPlayerAttack(mod, event);
            }
        );

        // Actorが攻撃された時のイベント
        mActorHurtListenerId = eventBus.emplaceListener<ll::event::ActorHurtEvent>(
            [this, &mod](ll::event::ActorHurtEvent& event) {
                onActorHurt(mod, event);
            }
        );

        mEnabled = true;
        mPlayer = std::make_unique<api::Player>(mod);

        mod.getSelf().getLogger().info("AttackInfo enabled - showing enemy name and HP on attack");
    }

    void onDisable(origin_mod::OriginMod& mod) override {
        if (!mEnabled) return;

        // イベントリスナーを削除
        if (mAttackListenerId) {
            auto& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener<ll::event::PlayerAttackEvent>(mAttackListenerId);
            mAttackListenerId = nullptr;
        }

        if (mActorHurtListenerId) {
            auto& eventBus = ll::event::EventBus::getInstance();
            eventBus.removeListener<ll::event::ActorHurtEvent>(mActorHurtListenerId);
            mActorHurtListenerId = nullptr;
        }

        mPlayer.reset();
        mEnabled = false;

        mod.getSelf().getLogger().info("AttackInfo disabled");
    }

private:
    bool mEnabled{false};
    ll::event::ListenerPtr mAttackListenerId{nullptr};
    ll::event::ListenerPtr mActorHurtListenerId{nullptr};
    std::unique_ptr<api::Player> mPlayer;

    void onPlayerAttack(origin_mod::OriginMod& mod, ll::event::PlayerAttackEvent& event) {
        if (!mEnabled || !mPlayer) return;
        try {
            auto& target = event.target();
            auto& player = event.self();

            // 攻撃情報を収集
            std::string targetName = target.getNameTag();
            if (targetName.empty()) {
                targetName = target.getFormattedNameTag();
            }
            if (targetName.empty()) {
                targetName = target.getTypeName();
            }

            std::string targetType = target.getTypeName();
            bool isTargetPlayer = target.isPlayer();

            // 位置情報と距離計算
            auto playerPos = player.getPosition();
            auto targetPos = target.getPosition();
            double distance = std::sqrt(
                std::pow(targetPos.x - playerPos.x, 2) +
                std::pow(targetPos.y - playerPos.y, 2) +
                std::pow(targetPos.z - playerPos.z, 2)
            );

            // World APIイベントを発行
            auto& world = api::World::instance();
            api::PlayerAttackEntityEventData attackEvent{
                .attackerName = player.getRealName(),
                .targetName = targetName,
                .targetType = targetType,
                .damage = 0.0f, // PlayerAttackEventには直接のダメージ情報がない
                .wasCritical = false, // 同上
                .distance = distance,
                .targetIsPlayer = isTargetPlayer
            };
            world.emitPlayerAttack(attackEvent);

            std::string message = fmt::format(
                "§e[Attack] §fTarget: {} ({}m)",
                targetName, static_cast<int>(distance)
            );

            mPlayer->localSendMessage(message);

        } catch (const std::exception& e) {
            mod.getSelf().getLogger().debug("AttackInfo: Error in player attack event: {}", e.what());
        } catch (...) {
            mod.getSelf().getLogger().debug("AttackInfo: Unknown error in player attack event");
        }
    }

    void onActorHurt(origin_mod::OriginMod& mod, ll::event::ActorHurtEvent& event) {
        if (!mEnabled || !mPlayer) return;

        try {
            // 基本的なダメージ情報表示（簡素版）
            std::string message = fmt::format(
                "§e[Hit] §fDamage: {:.1f}",
                event.damage()
            );

            mPlayer->localSendMessage(message);

        } catch (const std::exception& e) {
            mod.getSelf().getLogger().debug("AttackInfo: Error in actor hurt event: {}", e.what());
        } catch (...) {
            mod.getSelf().getLogger().debug("AttackInfo: Unknown error in actor hurt event");
        }
    }
};

} // namespace origin_mod::features

namespace {
void registerAttackInfo(origin_mod::features::FeatureRegistry& reg) {
    (void)reg.registerFeature<origin_mod::features::AttackInfoFeature>("attackinfo");
}
} // namespace

ORIGINMOD_REGISTER_FEATURE(registerAttackInfo);
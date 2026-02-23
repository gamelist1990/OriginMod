#pragma once

#include "ll/api/event/Cancellable.h"
#include "ll/api/event/player/PlayerEvent.h"
#include "mc/world/actor/ActorDamageSource.h"
#include "mc/deps/shared_types/legacy/actor/ActorDamageCause.h"

namespace origin_mod::api {

// AttackInfo用の攻撃イベント
class PlayerAttackEntityEvent final : public ll::event::PlayerEvent {
private:
    ::Actor* mTarget;
    ::ActorDamageSource const& mDamageSource;
    float mDamage;
    bool mWasCritical;
    ::SharedTypes::Legacy::ActorDamageCause mCause;

public:
    constexpr explicit PlayerAttackEntityEvent(
        ::Player& player,
        ::Actor* target,
        ::ActorDamageSource const& source,
        float damage,
        bool wasCritical,
        ::SharedTypes::Legacy::ActorDamageCause cause
    )
    : PlayerEvent(player),
      mTarget(target),
      mDamageSource(source),
      mDamage(damage),
      mWasCritical(wasCritical),
      mCause(cause) {}

    [[nodiscard]] ::Actor* getTarget() const { return mTarget; }
    [[nodiscard]] ::ActorDamageSource const& getDamageSource() const { return mDamageSource; }
    [[nodiscard]] float getDamage() const { return mDamage; }
    [[nodiscard]] bool wasCritical() const { return mWasCritical; }
    [[nodiscard]] ::SharedTypes::Legacy::ActorDamageCause getCause() const { return mCause; }

    // 攻撃対象の名前と種類を取得
    [[nodiscard]] std::string getTargetName() const;
    [[nodiscard]] std::string getTargetTypeName() const;
    [[nodiscard]] bool isTargetPlayer() const;

    // 座標情報
    struct AttackInfo {
        double attackerX, attackerY, attackerZ;
        double targetX, targetY, targetZ;
        double distance;
    };

    [[nodiscard]] AttackInfo getAttackInfo() const;
};

} // namespace origin_mod::api
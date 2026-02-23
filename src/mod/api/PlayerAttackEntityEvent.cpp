#include "mod/api/PlayerAttackEntityEvent.h"
#include <cmath>

namespace origin_mod::api {

std::string PlayerAttackEntityEvent::getTargetName() const {
    if (!mTarget) return "Unknown";

    std::string name = mTarget->getNameTag();
    if (name.empty()) {
        name = mTarget->getFormattedNameTag();
    }
    if (name.empty()) {
        name = mTarget->getTypeName();
    }
    return name;
}

std::string PlayerAttackEntityEvent::getTargetTypeName() const {
    if (!mTarget) return "Unknown";
    return mTarget->getTypeName();
}

bool PlayerAttackEntityEvent::isTargetPlayer() const {
    if (!mTarget) return false;
    return mTarget->isPlayer();
}

PlayerAttackEntityEvent::AttackInfo PlayerAttackEntityEvent::getAttackInfo() const {
    AttackInfo info{};

    // 攻撃者（プレイヤー）の位置
    auto playerPos = self().getPosition();
    info.attackerX = playerPos.x;
    info.attackerY = playerPos.y;
    info.attackerZ = playerPos.z;

    // 攻撃対象の位置
    if (mTarget) {
        auto targetPos = mTarget->getPosition();
        info.targetX = targetPos.x;
        info.targetY = targetPos.y;
        info.targetZ = targetPos.z;

        // 距離計算
        double dx = info.targetX - info.attackerX;
        double dy = info.targetY - info.attackerY;
        double dz = info.targetZ - info.attackerZ;
        info.distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    }

    return info;
}

} // namespace origin_mod::api
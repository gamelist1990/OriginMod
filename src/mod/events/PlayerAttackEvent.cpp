#include "mod/events/PlayerAttackEvent.h"
#include <cmath>

// MineCraft includes for entity access
#include "mc/world/actor/Actor.h"
#include "mc/world/actor/player/Player.h"

namespace origin_mod::events {

PlayerAttackEventData PlayerAttackEventHelper::createFromLeviLamina(
    const ::Player& attacker,
    const ::Actor& target
) {
    PlayerAttackEventData data{};

    // 基本情報
    data.attackerName = attacker.getRealName();
    data.targetName = getTargetName(target);
    data.targetType = target.getTypeName();
    data.targetIsPlayer = target.isPlayer();

    // 位置情報
    auto attackerPos = attacker.getPosition();
    auto targetPos = target.getPosition();

    data.attackerX = attackerPos.x;
    data.attackerY = attackerPos.y;
    data.attackerZ = attackerPos.z;

    data.targetX = targetPos.x;
    data.targetY = targetPos.y;
    data.targetZ = targetPos.z;

    // 距離計算
    data.distance = calculateDistance(
        data.attackerX, data.attackerY, data.attackerZ,
        data.targetX, data.targetY, data.targetZ
    );

    // ダメージとクリティカル（LeviLamina PlayerAttackEventには直接の情報がない）
    data.damage = 0.0;
    data.wasCritical = false;

    return data;
}

double PlayerAttackEventHelper::calculateDistance(
    double x1, double y1, double z1,
    double x2, double y2, double z2
) {
    double dx = x2 - x1;
    double dy = y2 - y1;
    double dz = z2 - z1;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

std::string PlayerAttackEventHelper::getTargetName(const ::Actor& target) {
    std::string name = target.getNameTag();
    if (!name.empty()) {
        return name;
    }

    name = target.getFormattedNameTag();
    if (!name.empty()) {
        return name;
    }

    return target.getTypeName();
}

} // namespace origin_mod::events
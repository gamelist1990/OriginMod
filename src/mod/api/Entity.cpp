#include "mod/api/Entity.h"
#include "mod/OriginMod.h"

#include "mc/world/actor/Actor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <mutex>
#include <unordered_map>

namespace origin_mod::api {

namespace {

// Remote entities' Actor::getHealth() can be unreliable on the client (often stuck at 20).
// We keep a best-effort estimate based on hurt events observed locally.
//
// Notes:
// - This is still an estimate (healing/regeneration/instant-health etc. are not tracked here).
// - For LocalPlayer, we always prefer Actor::getHealth().
class HealthEstimator {
public:
    static HealthEstimator& instance() {
        static HealthEstimator g;
        return g;
    }

    int getEstimatedHealth(::Actor& actor) {
        const auto now = std::chrono::steady_clock::now();

        // Seed / refresh from max health (more reliable than current health for remote actors).
        float maxHp = -1.0f;
        try {
            maxHp = static_cast<float>(actor.getMaxHealth());
        } catch (...) {
        }
        if (maxHp <= 0.0f) {
            maxHp = 20.0f;
        }

        const int64 uid = getActorUid(actor);

        std::scoped_lock lk(mMutex);
        auto& st = mStates[uid];
        if (st.maxHp <= 0.0f || std::abs(st.maxHp - maxHp) > 0.001f) {
            st.maxHp = maxHp;
            // If we don't have a current estimate yet, assume full.
            if (st.hp < 0.0f) {
                st.hp = st.maxHp;
                st.lastHealAt = now;
            } else {
                st.hp = std::clamp(st.hp, 0.0f, st.maxHp);
            }
        }

        if (st.hp < 0.0f) {
            st.hp = st.maxHp;
            st.lastHealAt = now;
        }

        // Natural regeneration (estimate): +1 HP every 4 seconds.
        // TargetHUDの実装に寄せて「4秒ごとに回復」だけを行う（空腹等は考慮しない）。
        constexpr auto kHealInterval = std::chrono::milliseconds(4000);
        if (st.hp < st.maxHp) {
            if (st.lastHealAt.time_since_epoch().count() == 0) {
                st.lastHealAt = now;
            } else {
                const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - st.lastHealAt);
                const int heals = static_cast<int>(elapsedMs.count() / kHealInterval.count());
                if (heals > 0) {
                    st.hp = std::min(st.hp + static_cast<float>(heals), st.maxHp);
                    st.lastHealAt += kHealInterval * heals;
                }
            }
        }

        return static_cast<int>(std::clamp(st.hp, 0.0f, st.maxHp) + 0.5f);
    }

    void applyDamage(::Actor& actor, float damage) {
        if (!(damage > 0.0f)) return;

        float maxHp = 20.0f;
        try {
            maxHp = static_cast<float>(actor.getMaxHealth());
            if (maxHp <= 0.0f) maxHp = 20.0f;
        } catch (...) {
        }

        const int64 uid = getActorUid(actor);
        const auto now = std::chrono::steady_clock::now();

        std::scoped_lock lk(mMutex);
        auto& st = mStates[uid];
        if (st.maxHp <= 0.0f) st.maxHp = maxHp;
        if (st.hp < 0.0f) st.hp = st.maxHp;
        st.hp = std::clamp(st.hp - damage, 0.0f, st.maxHp);
        st.lastUpdate = now;
        st.lastDamageAt = now;
    }

    void applyHeal(::Actor& actor, float heal) {
        if (!(heal > 0.0f)) return;

        float maxHp = 20.0f;
        try {
            maxHp = static_cast<float>(actor.getMaxHealth());
            if (maxHp <= 0.0f) maxHp = 20.0f;
        } catch (...) {
        }

        const int64 uid = getActorUid(actor);
        const auto now = std::chrono::steady_clock::now();

        std::scoped_lock lk(mMutex);
        auto& st = mStates[uid];
        if (st.maxHp <= 0.0f) st.maxHp = maxHp;
        if (st.hp < 0.0f) st.hp = st.maxHp;
        st.hp = std::clamp(st.hp + heal, 0.0f, st.maxHp);
        st.lastUpdate = now;
        st.lastHealAt = now;
    }

    void clear(::Actor& actor) {
        const int64 uid = getActorUid(actor);
        std::scoped_lock lk(mMutex);
        mStates.erase(uid);
    }

private:
    HealthEstimator() = default;

    struct State {
        float hp{-1.0f};
        float maxHp{-1.0f};
        std::chrono::steady_clock::time_point lastUpdate{};
        std::chrono::steady_clock::time_point lastDamageAt{};
        std::chrono::steady_clock::time_point lastHealAt{};
    };

    std::mutex mMutex;
    std::unordered_map<int64, State> mStates;

    static int64 getActorUid(::Actor& actor) {
        try {
            return actor.getOrCreateUniqueID().rawID;
        } catch (...) {
            // Fallback: use address as last resort (non-stable across sessions).
            return reinterpret_cast<int64>(&actor);
        }
    }

};

} // namespace

Entity::Entity(::Actor* actor, origin_mod::OriginMod& mod)
: mActor(actor), mMod(mod) {}

std::string Entity::getName() const {
    if (!mActor) return "";

    try {
        std::string name = mActor->getNameTag();
        if (!name.empty()) {
            return name;
        }
        // フォーマットされた名前タグを試す
        return mActor->getFormattedNameTag();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get entity name");
        return "";
    }
}

std::string Entity::getTypeName() const {
    if (!mActor) return "";

    try {
        return mActor->getTypeName();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get entity type name");
        return "";
    }
}

Entity::Location Entity::location() const {
    if (!mActor) return Location();

    try {
        auto pos = mActor->getPosition();
        return Location(pos.x, pos.y, pos.z);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get entity location");
        return Location();
    }
}

int Entity::getHealth() const {
    if (!mActor) return -1;

    try {
        // Local player health is accurate.
        try {
            if (mActor->isLocalPlayer()) {
                return mActor->getHealth();
            }
        } catch (...) {
            // ignore
        }

        // Fallback death detection:
        // Some environments still update remote actors' health to 0 on death.
        // If we can observe that, clear estimation so the next encounter re-seeds to max.
        try {
            if (!mActor->isAlive()) {
                clearHealthEstimate(mActor);
                return 0;
            }
        } catch (...) {
        }

        // For remote entities, Actor::getHealth() is often unreliable on the client.
        // Use hurt-event-based estimate.
        return HealthEstimator::instance().getEstimatedHealth(*mActor);
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get entity health");
        return -1;
    }
}

void Entity::applyDamageEstimate(::Actor* actor, float damage) {
    if (!actor) return;
    try {
        HealthEstimator::instance().applyDamage(*actor, damage);
    } catch (...) {
        // ignore
    }
}

void Entity::applyHealEstimate(::Actor* actor, float heal) {
    if (!actor) return;
    try {
        HealthEstimator::instance().applyHeal(*actor, heal);
    } catch (...) {
        // ignore
    }
}

void Entity::clearHealthEstimate(::Actor* actor) {
    if (!actor) return;
    try {
        HealthEstimator::instance().clear(*actor);
    } catch (...) {
        // ignore
    }
}

bool Entity::isValid() const {
    return mActor != nullptr;
}

bool Entity::isAlive() const {
    if (!mActor) return false;

    try {
        return mActor->isAlive();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to check if entity is alive");
        return false;
    }
}

bool Entity::isPlayer() const {
    if (!mActor) return false;

    try {
        return mActor->isPlayer();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to check if entity is player");
        return false;
    }
}

} // namespace origin_mod::api
#include "mod/events/DamageTracker.h"

#include "mod/OriginMod.h"
#include "mod/api/Entity.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/entity/ActorHurtEvent.h"

#include "mc/world/actor/Actor.h"

#include "ll/api/service/Bedrock.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/Level.h"

#include <algorithm>
#include <type_traits>
#include <utility>

namespace origin_mod::events {

namespace {
template <class T, class = void>
struct StorageReader {
    static float read(T const& v) {
        return static_cast<float>(v);
    }
};

template <class T>
struct StorageReader<T, std::void_t<decltype(std::declval<T const&>().get())>> {
    static float read(T const& v) {
        return static_cast<float>(v.get());
    }
};
} // namespace

DamageTracker& DamageTracker::instance() {
    static DamageTracker g;
    return g;
}

void DamageTracker::initialize(origin_mod::OriginMod& mod) {
    // Idempotent
    if (mHurtListener || mAttackListener) {
        mMod = &mod;
        return;
    }

    mMod = &mod;

    try {
        auto& bus = ll::event::EventBus::getInstance();
        mHurtListener = bus.emplaceListener<ll::event::ActorHurtEvent>(
            [this](ll::event::ActorHurtEvent& ev) {
                onActorHurt(ev);
            }
        );

        // Fallback: if ActorHurtEvent doesn't fire on the client in some environments,
        // use PlayerAttackEvent as a signal to move estimated HP.
        mAttackListener = bus.emplaceListener<ll::event::PlayerAttackEvent>(
            [this](ll::event::PlayerAttackEvent& ev) {
                try {
                    auto& attacker = ev.self();
                    auto& victim = ev.target();

                    // only remote players need estimation
                    if (!victim.isPlayer() || victim.isLocalPlayer()) return;

                    const int64_t victimUid = tryGetUid(victim);
                    if (victimUid == 0) return;

                    const int64_t attackerUid = tryGetUid(attacker);
                    const auto now = std::chrono::steady_clock::now();

                    // If we recently saw/processed a real hurt update, don't queue.
                    {
                        std::scoped_lock lk(mMutex);
                        auto itH = mRecentByVictim.find(victimUid);
                        if (itH != mRecentByVictim.end()) {
                            if (now - itH->second.at < std::chrono::milliseconds(250)) {
                                return;
                            }
                        }
                        auto itA = mLastAppliedEstimateAt.find(victimUid);
                        if (itA != mLastAppliedEstimateAt.end()) {
                            if (now - itA->second < std::chrono::milliseconds(150)) {
                                return;
                            }
                        }
                    }

                    // Try to reuse a recent hurt record if any (damage amount).
                    float dmg = 1.0f;
                    {
                        auto rec = getRecentDamage(victimUid, std::chrono::milliseconds(500), attackerUid);
                        if (rec.has_value() && rec->damage > 0.0f) {
                            dmg = rec->damage;
                        }
                    }

                    // Simple crit estimation
                    bool crit = false;
                    try {
                        if (!attacker.isOnGround() && attacker.getFallDistance() > 0.0f && !attacker.isInWater() && !attacker.isInLava()) {
                            crit = true;
                        }
                    } catch (...) {
                    }

                    const float dmgToApply = crit ? (dmg * 1.5f) : dmg;

                    // Queue pending hit; apply only when we observe a confirmed HURT signal.
                    {
                        std::scoped_lock lk(mMutex);
                        mPendingByVictim[victimUid] = PendingHit{attackerUid, dmgToApply, now};
                    }

                } catch (...) {
                    // ignore
                }
            }
        );

        mod.getSelf().getLogger().info("DamageTracker: subscribed to ActorHurtEvent");
    } catch (...) {
        mod.getSelf().getLogger().warn("DamageTracker: failed to subscribe ActorHurtEvent");
    }
}

void DamageTracker::shutdown() {
    if (!mHurtListener && !mAttackListener) {
        mMod = nullptr;
        return;
    }

    try {
        auto& bus = ll::event::EventBus::getInstance();
        if (mHurtListener) bus.removeListener<ll::event::ActorHurtEvent>(mHurtListener);
        if (mAttackListener) bus.removeListener<ll::event::PlayerAttackEvent>(mAttackListener);
    } catch (...) {
        // ignore
    }

    mHurtListener = nullptr;
    mAttackListener = nullptr;
    mMod = nullptr;

    std::scoped_lock lk(mMutex);
    mRecentByVictim.clear();
    mLastAppliedEstimateAt.clear();
    mPendingByVictim.clear();
}

std::optional<DamageTracker::DamageRecord>
DamageTracker::getRecentDamage(int64_t victimUid, std::chrono::milliseconds maxAge, int64_t attackerUid) const {
    if (victimUid == 0) return std::nullopt;

    const auto now = std::chrono::steady_clock::now();

    std::scoped_lock lk(mMutex);
    auto it = mRecentByVictim.find(victimUid);
    if (it == mRecentByVictim.end()) return std::nullopt;

    const auto& rec = it->second;
    if (now - rec.at > maxAge) return std::nullopt;
    if (attackerUid != 0 && rec.attackerUid != 0 && rec.attackerUid != attackerUid) return std::nullopt;
    return rec;
}

int64_t DamageTracker::tryGetUid(::Actor& a) {
    try {
        return a.getOrCreateUniqueID().rawID;
    } catch (...) {
        return 0;
    }
}

void DamageTracker::onActorHurt(ll::event::ActorHurtEvent& ev) {
    try {
        auto& victim = ev.self();
        const float dmg = ev.damage();
        if (!(dmg > 0.0f)) return;

        const int64_t victimUid = tryGetUid(victim);
        if (victimUid == 0) return;

        int64_t attackerUid = 0;
        try {
            attackerUid = ev.source().getDamagingEntityUniqueID().rawID;
        } catch (...) {
        }

        DamageRecord rec;
        rec.victimUid = victimUid;
        rec.attackerUid = attackerUid;
        rec.damage = dmg;
        rec.wasCritical = false; // not exposed here
        rec.at = std::chrono::steady_clock::now();

        {
            std::scoped_lock lk(mMutex);
            mRecentByVictim[victimUid] = rec;
            mLastAppliedEstimateAt[victimUid] = rec.at;
            mPendingByVictim.erase(victimUid);
        }

        // Feed the estimation cache for remote entities.
        origin_mod::api::Entity::applyDamageEstimate(&victim, dmg);

    } catch (...) {
        // ignore
    }
}

namespace {
::Actor* tryResolveActorByRuntimeId(::ActorRuntimeID rid) {
    try {
        auto ciOpt = ll::service::bedrock::getClientInstance();
        if (!ciOpt) return nullptr;
        auto* lp = ciOpt->getLocalPlayer();
        if (!lp) return nullptr;
        auto* level = &lp->getLevel();
        if (!level) return nullptr;

        auto actors = level->getRuntimeActorList();
        for (auto* a : actors) {
            if (!a) continue;
            try {
                if (a->getRuntimeID().rawID == rid.rawID) return a;
            } catch (...) {
            }
        }
    } catch (...) {
    }
    return nullptr;
}
} // namespace

void DamageTracker::onActorEventPacketHurt(::ActorRuntimeID runtimeId) {
    // Confirmed "victim was hurt" from the network.
    // Apply at most one queued hit.
    try {
        ::Actor* victim = tryResolveActorByRuntimeId(runtimeId);
        if (!victim) return;

        const int64_t victimUid = tryGetUid(*victim);
        if (victimUid == 0) return;

        PendingHit ph;
        {
            std::scoped_lock lk(mMutex);
            auto it = mPendingByVictim.find(victimUid);
            if (it == mPendingByVictim.end()) return;
            ph = it->second;
            // stale pending -> drop
            if (std::chrono::steady_clock::now() - ph.at > std::chrono::milliseconds(800)) {
                mPendingByVictim.erase(it);
                return;
            }
            mPendingByVictim.erase(it);
        }

        // Try to get a better damage number from the victim's last hurt amount.
        // This tends to be updated when the client receives a HURT signal.
        float lastHurt = 0.0f;
        try {
            // 環境によって mLastHurt が TypedStorage の場合と、生の float として見える場合がある
            using LastHurtT = std::remove_reference_t<decltype(victim->mLastHurt)>;
            lastHurt = StorageReader<LastHurtT>::read(victim->mLastHurt);
        } catch (...) {
        }
        if (lastHurt > 0.0f && lastHurt < 200.0f) {
            ph.damageToApply = lastHurt;
        }

        if (ph.damageToApply > 0.0f) {
            origin_mod::api::Entity::applyDamageEstimate(victim, ph.damageToApply);

            const auto now = std::chrono::steady_clock::now();
            {
                std::scoped_lock lk(mMutex);
                mLastAppliedEstimateAt[victimUid] = now;
                // Also store as a recent damage record so AttackInfo can show DMG even without ActorHurtEvent.
                DamageRecord rec;
                rec.victimUid = victimUid;
                rec.attackerUid = ph.attackerUid;
                rec.damage = ph.damageToApply;
                rec.wasCritical = false;
                rec.at = now;
                mRecentByVictim[victimUid] = rec;
            }
        }

    } catch (...) {
        // ignore
    }
}

void DamageTracker::onActorEventPacketDeath(::ActorRuntimeID runtimeId) {
    try {
        ::Actor* victim = tryResolveActorByRuntimeId(runtimeId);
        if (!victim) return;

        const int64_t victimUid = tryGetUid(*victim);
        if (victimUid != 0) {
            std::scoped_lock lk(mMutex);
            mRecentByVictim.erase(victimUid);
            mPendingByVictim.erase(victimUid);
            mLastAppliedEstimateAt.erase(victimUid);
        }

        // Reset estimated HP so the next time we see this actor we seed from max (usually 20).
        origin_mod::api::Entity::clearHealthEstimate(victim);

    } catch (...) {
        // ignore
    }
}

} // namespace origin_mod::events

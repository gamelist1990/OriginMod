#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "ll/api/event/Listener.h"
#include "ll/api/event/entity/ActorHurtEvent.h"
#include "ll/api/event/player/PlayerAttackEvent.h"

#include "mc/legacy/ActorRuntimeID.h"

// Forward declarations
namespace origin_mod {
class OriginMod;
}

class Actor;

namespace origin_mod::events {

class DamageTracker {
public:
    struct DamageRecord {
        int64_t victimUid{0};
        int64_t attackerUid{0};
        float damage{0.0f};
        bool wasCritical{false};
        std::chrono::steady_clock::time_point at{};
    };

    static DamageTracker& instance();

    // Subscribe to events. Safe to call multiple times.
    void initialize(origin_mod::OriginMod& mod);

    // Unsubscribe. Safe to call multiple times.
    void shutdown();

    // Confirmed hurt signal from network packet (ActorEventPacket::Hurt).
    // This is useful to avoid over-subtraction when CPS is high and attack events fire on misses.
    void onActorEventPacketHurt(::ActorRuntimeID runtimeId);

    // Confirmed death signal from network packet (ActorEventPacket::Death).
    // We clear estimation caches so the next encounter starts from max health.
    void onActorEventPacketDeath(::ActorRuntimeID runtimeId);

    // Lookup the most recent damage seen for the victim.
    // Optionally filter by attackerUid (0 = don't care).
    [[nodiscard]] std::optional<DamageRecord>
    getRecentDamage(int64_t victimUid, std::chrono::milliseconds maxAge, int64_t attackerUid = 0) const;

    // Best-effort actor unique id extraction.
    static int64_t tryGetUid(::Actor& a);

private:
    DamageTracker() = default;

    void onActorHurt(ll::event::ActorHurtEvent& ev);

private:
    mutable std::mutex mMutex;
    std::unordered_map<int64_t, DamageRecord> mRecentByVictim;

    // To avoid double-applying (hurt + attack fallback)
    std::unordered_map<int64_t, std::chrono::steady_clock::time_point> mLastAppliedEstimateAt;

    struct PendingHit {
        int64_t attackerUid{0};
        float damageToApply{0.0f};
        std::chrono::steady_clock::time_point at{};
    };
    // Victim uid -> pending hit. Applied only when we see a confirmed hurt signal.
    std::unordered_map<int64_t, PendingHit> mPendingByVictim;

    ll::event::ListenerPtr mHurtListener{nullptr};
    ll::event::ListenerPtr mAttackListener{nullptr};
    origin_mod::OriginMod* mMod{nullptr};
};

} // namespace origin_mod::events

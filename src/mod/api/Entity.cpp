#include "mod/api/Entity.h"
#include "mod/OriginMod.h"

#include "mc/world/actor/Actor.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "ll/api/service/Bedrock.h"

#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"
#include "mc/world/level/Level.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace origin_mod::api {

namespace {

class DeathMomentTracker {
public:
    static DeathMomentTracker& instance() {
        static DeathMomentTracker g;
        g.ensureSubscribed();
        return g;
    }

    bool consumeDeathMoment(::Actor& actor) {
        ensureSubscribed();
        const int64 uid = getActorUid(actor);
        if (uid == 0) return false;

        const auto now = std::chrono::steady_clock::now();
        std::scoped_lock lk(mMutex);
        auto it = mDeathAt.find(uid);
        if (it == mDeathAt.end()) return false;
        // TTL: allow consumers a short window to observe
        if (now - it->second > std::chrono::milliseconds(1500)) {
            mDeathAt.erase(it);
            return false;
        }
        mDeathAt.erase(it);
        return true;
    }

private:
    DeathMomentTracker() = default;

    std::mutex mMutex;
    std::unordered_map<int64, int> mLastHp;
    std::unordered_map<int64, std::chrono::steady_clock::time_point> mDeathAt;
    ll::event::ListenerPtr mTickListener{nullptr};
    std::once_flag mSubscribeOnce;

    static int64 getActorUid(::Actor& actor) {
        try {
            return actor.getOrCreateUniqueID().rawID;
        } catch (...) {
            return reinterpret_cast<int64>(&actor);
        }
    }

    void ensureSubscribed() {
        std::call_once(mSubscribeOnce, [this]() {
            try {
                auto& bus = ll::event::EventBus::getInstance();
                mTickListener = bus.emplaceListener<ll::event::LevelTickEvent>(
                    [this](ll::event::LevelTickEvent&) {
                        onTick();
                    }
                );
            } catch (...) {
                // ignore
            }
        });
    }

    void onTick() {
        try {
            auto ciOpt = ll::service::bedrock::getClientInstance();
            if (!ciOpt) return;
            auto* lp = ciOpt->getLocalPlayer();
            if (!lp) return;
            ::Level* level = &lp->getLevel();
            if (!level) return;

            auto actors = level->getRuntimeActorList();
            const auto now = std::chrono::steady_clock::now();

            std::scoped_lock lk(mMutex);

            for (auto* a : actors) {
                if (!a) continue;

                int hp = -1;
                try {
                    hp = a->getHealth();
                } catch (...) {
                    continue;
                }

                const int64 uid = getActorUid(*a);
                if (uid == 0) continue;

                int& last = mLastHp[uid];
                if (last > 0 && hp == 0) {
                    mDeathAt[uid] = now;
                }
                last = hp;
            }

            // prune old flags
            for (auto it = mDeathAt.begin(); it != mDeathAt.end();) {
                if (now - it->second > std::chrono::seconds(3)) {
                    it = mDeathAt.erase(it);
                } else {
                    ++it;
                }
            }

        } catch (...) {
            // ignore
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
        return mActor->getHealth();
    } catch (...) {
        mMod.getSelf().getLogger().debug("Failed to get entity health");
        return -1;
    }
}

bool Entity::isDeath() const {
    if (!mActor) return false;
    try {
        return DeathMomentTracker::instance().consumeDeathMoment(*mActor);
    } catch (...) {
        return false;
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
#include "mod/api/Entity.h"
#include "mod/OriginMod.h"

#include "mc/world/actor/Actor.h"

namespace origin_mod::api {

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
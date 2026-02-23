#pragma once

#include <string>
#include "mc/world/actor/Actor.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::api {

// Entity wrapper for general actors (mobs, items, etc.)
class Entity {
public:
    // Location structure for position data
    struct Location {
        double x, y, z;
        Location(double x = 0.0, double y = 0.0, double z = 0.0) : x(x), y(y), z(z) {}
    };

    explicit Entity(::Actor* actor, origin_mod::OriginMod& mod);

    [[nodiscard]] origin_mod::OriginMod& mod() const noexcept { return mMod; }

    // Entity identification
    [[nodiscard]] std::string getName() const;
    [[nodiscard]] std::string getTypeName() const;

    // Position and health utilities
    [[nodiscard]] Location location() const;
    [[nodiscard]] int getHealth() const;

    // True only when we observe the moment this actor's health becomes 0 (death moment).
    // Returns true once per death (consumed).
    [[nodiscard]] bool isDeath() const;

    // Entity state checks
    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isAlive() const;
    [[nodiscard]] bool isPlayer() const;

    // Raw actor access (for advanced operations)
    [[nodiscard]] ::Actor* getRawActor() const noexcept { return mActor; }

private:
    ::Actor* mActor;
    origin_mod::OriginMod& mMod;
};

} // namespace origin_mod::api
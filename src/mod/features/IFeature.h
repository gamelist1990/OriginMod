#pragma once

#include <string_view>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::features {

class IFeature {
public:
    virtual ~IFeature() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept          = 0; // stable key (lowercase recommended)
    [[nodiscard]] virtual std::string_view name() const noexcept        = 0;
    [[nodiscard]] virtual std::string_view description() const noexcept = 0;

    virtual void onEnable(origin_mod::OriginMod&) {}
    virtual void onDisable(origin_mod::OriginMod&) {}
    virtual void onTick(origin_mod::OriginMod&) {}
};

} // namespace origin_mod::features

#pragma once

#include <string_view>

namespace origin_mod {
class OriginMod;

namespace modules {

class IModule {
public:
    virtual ~IModule() = default;

    [[nodiscard]] virtual std::string_view id() const noexcept = 0;

    // Called once when the mod is loaded.
    [[nodiscard]] virtual bool load(OriginMod&) { return true; }

    // Called when the mod is enabled.
    [[nodiscard]] virtual bool enable(OriginMod&) { return true; }

    // Called when the mod is disabled.
    [[nodiscard]] virtual bool disable(OriginMod&) { return true; }
};

} // namespace modules
} // namespace origin_mod

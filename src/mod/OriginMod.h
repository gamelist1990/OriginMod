#pragma once

#include "ll/api/mod/NativeMod.h"

namespace origin_mod {

class OriginMod {
public:
    static OriginMod& getInstance();

    [[nodiscard]] ll::mod::NativeMod& getSelf() const;

    /// @return True if the mod loaded successfully.
    bool load();

    /// @return True if the mod enabled successfully.
    bool enable();

    /// @return True if the mod disabled successfully.
    bool disable();

private:
    OriginMod() = default;
};

} // namespace origin_mod
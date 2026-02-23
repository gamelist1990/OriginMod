#pragma once

#include <memory>
#include <vector>

#include "ll/api/mod/NativeMod.h"

#include "mod/modules/IModule.h"

namespace origin_mod {

class OriginMod {

public:
    static OriginMod& getInstance();

    OriginMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    /// @return True if the mod is loaded successfully.
    bool load();

    /// @return True if the mod is enabled successfully.
    bool enable();

    /// @return True if the mod is disabled successfully.
    bool disable();

    // TODO: Implement this method if you need to unload the mod.
    // /// @return True if the mod is unloaded successfully.
    // bool unload();

private:
    ll::mod::NativeMod& mSelf;

    std::vector<std::unique_ptr<modules::IModule>> mModules;

    [[nodiscard]] bool loadModules();
};

} // namespace origin_mod

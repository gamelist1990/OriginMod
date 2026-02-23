#include "mod/OriginMod.h"
#include "mod/util/DebugLogger.h"

#include <utility>

#include "ll/api/mod/RegisterHelper.h"

#include "mod/modules/ModuleRegistry.h"

namespace origin_mod {

OriginMod& OriginMod::getInstance() {
    static OriginMod instance;
    return instance;
}

bool OriginMod::load() {
    //Debugを有効にするときはTrueにね
    origin_mod::util::setDebugEnabled(true);

    getSelf().getLogger().debug("Loading...");

    return loadModules();
}

bool OriginMod::enable() {
    getSelf().getLogger().debug("Enabling...");

    for (auto& m : mModules) {
        if (!m->enable(*this)) {
            getSelf().getLogger().error("Module enable failed: {}", m->id());
            return false;
        }
    }
    return true;
}

bool OriginMod::disable() {
    getSelf().getLogger().debug("Disabling...");

    // Disable in reverse order.
    for (auto it = mModules.rbegin(); it != mModules.rend(); ++it) {
        auto& m = *it;
        if (!m->disable(*this)) {
            getSelf().getLogger().error("Module disable failed: {}", m->id());
            return false;
        }
    }
    return true;
}

bool OriginMod::loadModules() {
    mModules = modules::ModuleRegistry::instance().createAll();
    getSelf().getLogger().debug("Modules discovered: {}", mModules.size());

    for (auto& m : mModules) {
        getSelf().getLogger().debug("Loading module: {}", m->id());
        if (!m->load(*this)) {
            getSelf().getLogger().error("Module load failed: {}", m->id());
            return false;
        }
    }
    return true;
}

} // namespace origin_mod

LL_REGISTER_MOD(origin_mod::OriginMod, origin_mod::OriginMod::getInstance());

#include "mod/modules/WorldEventModule.h"

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"

#include "mod/OriginMod.h"
#include "mod/api/World.h"
#include "mod/modules/ModuleRegistry.h"

namespace origin_mod::modules {

bool WorldEventModule::load(OriginMod&) {
    return true;
}

bool WorldEventModule::enable(OriginMod& mod) {
    auto& bus = ll::event::EventBus::getInstance();

    mTickListener = bus.emplaceListener<ll::event::world::LevelTickEvent>(
        [this](ll::event::world::LevelTickEvent&) {
            origin_mod::api::TickEvent ev{.tick = ++mTickCounter};
            origin_mod::api::World::instance().emitTick(ev);
        },
        ll::event::EventPriority::Low
    );

    if (!mTickListener) {
        mod.getSelf().getLogger().warn("WorldEventModule: tick listener registration failed");
    }

    mod.getSelf().getLogger().debug("WorldEventModule enabled");
    return true;
}

bool WorldEventModule::disable(OriginMod& mod) {
    if (mTickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::world::LevelTickEvent>(mTickListener);
        mTickListener.reset();
    }
    mod.getSelf().getLogger().debug("WorldEventModule disabled");
    return true;
}

} // namespace origin_mod::modules

ORIGINMOD_REGISTER_MODULE(origin_mod::modules::WorldEventModule);

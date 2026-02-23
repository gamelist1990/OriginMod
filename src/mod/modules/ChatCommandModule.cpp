#include "mod/modules/ChatCommandModule.h"

#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "mod/hooks/OutgoingChatHook.h"

#include "mod/OriginMod.h"
#include "mod/api/World.h"
#include "mod/commands/OriginCommandRegistrar.h"
#include "mod/commands/OriginCommandTypes.h"
#include "mod/features/FeatureManager.h"
#include "mod/modules/ModuleRegistry.h"

namespace origin_mod::modules {


bool NativeCommandModule::dispatchOriginSubcommand(
    OriginMod& mod,
    std::string_view sub,
    std::vector<std::string> const& args,
    ::origin_mod::commands::OriginCommandRegistry::ReplyFn reply
) {
    buildRegistry();
    return mRegistry.execute(mod, sub, args, std::move(reply));
}

bool NativeCommandModule::load(OriginMod&) {
    // Defer actual registration to enable(), when game services are ready.
    return true;
}

void NativeCommandModule::buildRegistry() {
    if (mRegistryBuilt) {
        return;
    }
    mRegistry = {};
    ::origin_mod::commands::OriginCommandRegistrar::instance().applyAll(mRegistry);
    mRegistryBuilt = true;
}

static std::vector<std::string> collectSubcommandNames(::origin_mod::commands::OriginCommandRegistry const& reg) {
    std::vector<std::string> out;
    for (auto const& meta : reg.list()) {
        out.emplace_back(meta.name);
    }
    return out;
}

void NativeCommandModule::installCommands(OriginMod& mod) {
    buildRegistry();

    // Ensure feature manager is initialized for any command handlers that need it.
    auto& fm = features::FeatureManager::instance();
    fm.initialize(mod);

    mod.getSelf().getLogger().debug("chat-based origin commands installed (subcommands={})", mRegistry.list().size());
}

bool NativeCommandModule::enable(OriginMod& mod) {
    // Initial install.
    installCommands(mod);

    // If the client joins a level (local or remote server), the command registry may be refreshed.
    // Reinstall to keep /origin available.
    auto& bus = ll::event::EventBus::getInstance();

    mStartJoinListener = bus.emplaceListener<ll::event::client::ClientStartJoinLevelEvent>(
        [this, &mod](ll::event::client::ClientStartJoinLevelEvent&) { installCommands(mod); },
        ll::event::EventPriority::Low
    );

    mJoinListener = bus.emplaceListener<ll::event::client::ClientJoinLevelEvent>(
        [this, &mod](ll::event::client::ClientJoinLevelEvent&) { installCommands(mod); },
        ll::event::EventPriority::Low
    );

    // Install outgoing chat hook.
    ::origin_mod::hooks::installOutgoingChatHook(this, &mod);

    if (!mStartJoinListener || !mJoinListener) {
        mod.getSelf().getLogger().warn("Chat command listeners registration failed");
    }

    mod.getSelf().getLogger().debug("Chat command module enabled");
    return true;
}

bool NativeCommandModule::disable(OriginMod& mod) {
    if (mStartJoinListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::client::ClientStartJoinLevelEvent>(mStartJoinListener);
        mStartJoinListener.reset();
    }
    if (mJoinListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::client::ClientJoinLevelEvent>(mJoinListener);
        mJoinListener.reset();
    }

    ::origin_mod::hooks::uninstallOutgoingChatHook();

    mod.getSelf().getLogger().debug("Chat command module disabled");
    return true;
}

} // namespace origin_mod::modules

ORIGINMOD_REGISTER_MODULE(::origin_mod::modules::NativeCommandModule);

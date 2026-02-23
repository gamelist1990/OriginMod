#pragma once

#include "ll/api/event/ListenerBase.h"

#include "mod/commands/OriginCommandRegistry.h"
#include "mod/modules/IModule.h"

namespace origin_mod::modules {

class NativeCommandModule final : public IModule {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "native-commands"; }

    [[nodiscard]] bool load(OriginMod& mod) override;
    [[nodiscard]] bool enable(OriginMod& mod) override;
    [[nodiscard]] bool disable(OriginMod& mod) override;

    // Called from outgoing-chat hook to execute a subcommand.
    bool dispatchOriginSubcommand(
        OriginMod& mod,
        std::string_view sub,
        std::vector<std::string> const& args,
        origin_mod::commands::OriginCommandRegistry::ReplyFn reply
    );

private:
    origin_mod::commands::OriginCommandRegistry mRegistry;
    bool                                    mRegistryBuilt{false};

    // Reinstall when client joins a level (local world or external server).
    ll::event::ListenerPtr mStartJoinListener;
    ll::event::ListenerPtr mJoinListener;

    void buildRegistry();
    void installCommands(OriginMod& mod);
};

} // namespace origin_mod::modules

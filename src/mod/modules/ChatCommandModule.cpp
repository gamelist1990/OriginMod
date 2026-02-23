#include "mod/modules/ChatCommandModule.h"

#include <string>
#include <vector>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/client/ClientJoinLevelEvent.h"
#include "ll/api/event/client/ClientStartJoinLevelEvent.h"
#include "mod/hooks/OutgoingChatHook.h"

#include "mod/OriginMod.h"
#include "mod/api/World.h"
#include "mod/commands/BuiltinCommands.h"
#include "mod/commands/OriginCommandRegistrar.h"
#include "mod/commands/OriginCommandTypes.h"
#include "mod/features/FeatureManager.h"
#include "mod/modules/ModuleRegistry.h"

#include <fmt/format.h>

namespace origin_mod::modules {


bool NativeCommandModule::dispatchOriginSubcommand(
    OriginMod& mod,
    std::string_view sub,
    std::vector<std::string> const& args,
    ::origin_mod::commands::OriginCommandRegistry::ReplyFn reply
) {
    buildRegistry(mod);
    bool ok = mRegistry.execute(mod, sub, args, std::move(reply));
    if (!ok) {
        auto list = mRegistry.list();

        // User-visible diagnostics (debug logs may be disabled).
        // Show a compact list so we can quickly confirm whether commands are registered.
        std::string names;
        constexpr size_t kMaxItems = 12;
        constexpr size_t kMaxLen   = 180;
        for (size_t i = 0; i < list.size() && i < kMaxItems; ++i) {
            if (!names.empty()) names.append(", ");
            names.append(list[i].name);
            if (names.size() > kMaxLen) {
                names.append("...");
                break;
            }
        }

        // We can't assume caller will print error, so reply here.
        // Note: reply should be local-only for chat commands.
        auto logger = &mod.getSelf().getLogger();
        logger->warn("chat command not found: '{}', registry size={}", sub, list.size());
        if (!names.empty()) {
            logger->warn("registered commands: {}", names);
        }

        // Also show to player.
        origin_mod::api::Player{mod}.localSendMessage(
            fmt::format("§cコマンドが見つかりません: §f{} §7(registry size={})", std::string{sub}, list.size())
        );
        if (!names.empty()) {
            origin_mod::api::Player{mod}.localSendMessage(fmt::format("§7登録済み: §f{}", names));
        }
    }
    return ok;
}

bool NativeCommandModule::load(OriginMod&) {
    // Defer actual registration to enable(), when game services are ready.
    return true;
}

void NativeCommandModule::buildRegistry(OriginMod& mod) {
    // Always rebuild the registry.  Static initialization order is not
    // guaranteed across translation units, so command registration functions
    // may be added after the first call.  Recomputing on each invocation keeps
    // the list up to date and costs only a handful of vector copies.
    mRegistry = {};

    // Register built-ins explicitly (do not rely on static initialization).
    auto initialSize = mRegistry.list().size();
    ::origin_mod::commands::registerBuiltinCommands(mRegistry);
    auto afterBuiltinSize = mRegistry.list().size();

    // Apply additional registered commands
    auto& registrar = ::origin_mod::commands::OriginCommandRegistrar::instance();
    registrar.applyAll(mRegistry);
    auto finalSize = mRegistry.list().size();

    // Debug logging to track command registration
    auto& logger = mod.getSelf().getLogger();
    logger.debug("Command registry build: initial={}, after builtins={}, final={}",
                initialSize, afterBuiltinSize, finalSize);

    if (finalSize == 0) {
        logger.warn("Command registry is empty after build - this indicates a registration problem");
    } else {
        logger.debug("Successfully registered {} commands", finalSize);
        auto commands = mRegistry.list();
        for (const auto& cmd : commands) {
            logger.debug("  - {}: {}", cmd.name, cmd.description);
        }
    }

    mRegistryBuilt = true; // record that we've at least built once
}

void NativeCommandModule::installCommands(OriginMod& mod) {
    buildRegistry(mod);

    // One-time visible diagnostics: confirm registry isn't empty.
    // (Debug logs may not be visible depending on the user's logger settings.)
    if (!mAnnouncedRegistry) {
        mAnnouncedRegistry = true;
        auto const size = mRegistry.list().size();
        if (size == 0) {
            origin_mod::api::Player{mod}.localSendMessage(
                "§c[OriginMod] コマンドレジストリが空です (size=0)。help/toggle が登録されていません。"
            );
            origin_mod::api::Player{mod}.localSendMessage(
                "§7→ DLL が最新か / mod の読み込み先が bin/OriginMod/OriginMod.dll か確認してね"
            );
        } else {
            origin_mod::api::Player{mod}.localSendMessage(
                fmt::format("§7[OriginMod] チャットコマンド登録: §f{} §7件", size)
            );
        }
    }

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

ORIGINMOD_REGISTER_MODULE(origin_mod::modules::NativeCommandModule);

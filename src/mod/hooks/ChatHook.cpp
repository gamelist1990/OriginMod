#include "mod/hooks/ChatHook.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/Player.h"
#include <fmt/format.h>

namespace origin_mod::hooks {

void initializeChatHook(OriginMod& mod) {
    mod.getSelf().getLogger().debug("Initializing chat hook...");

    // CommandManagerを初期化
    auto& cmdManager = commands::CommandManager::getInstance();
    cmdManager.initialize();

    auto commands = cmdManager.getCommands();
    mod.getSelf().getLogger().info("Chat commands registered: {} commands", commands.size());

    for (const auto& cmd : commands) {
        mod.getSelf().getLogger().debug("  - {}: {}", cmd.name, cmd.description);
    }

    // TODO: 実際のチャットフック実装を後で追加
    // 今はコマンドマネージャーの初期化のみ

    mod.getSelf().getLogger().info("Chat hook initialized successfully - {} commands available", commands.size());
}

void shutdownChatHook() {
    // フックの解除処理
}

} // namespace origin_mod::hooks


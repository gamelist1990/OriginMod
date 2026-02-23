#include "mod/hooks/ChatHook.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include <fmt/format.h>

// LeviLamina event system includes
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/player/PlayerChatEvent.h"
#include "ll/api/event/world/LevelTickEvent.h"

// パケットフック用のインクルード
#include "ll/api/memory/Hook.h"
#include "mc/network/packet/TextPacket.h"

namespace origin_mod::hooks {

// グローバルリスナーポインターを保持
static ll::event::ListenerPtr chatListener = nullptr;
static ll::event::ListenerPtr tickListener = nullptr;

// パケットフック用のグローバル変数
static std::atomic<OriginMod*> gChatHookMod{nullptr};

// TextPacket受信フック（サーバーからクライアントへのチャットメッセージ）
LL_TYPE_INSTANCE_HOOK(
    TextPacketReceiveHook,
    ll::memory::HookPriority::Normal,
    TextPacket,
    static_cast<::Bedrock::Result<void> (TextPacket::*)(::ReadOnlyBinaryStream&)>(&TextPacket::$_read),
    ::Bedrock::Result<void>,
    ::ReadOnlyBinaryStream& stream
) {
    auto res = origin(stream);
    if (res) {
        // パケット処理後にカスタムイベントを発行
        auto* mod = gChatHookMod.load(std::memory_order_relaxed);
        if (mod) {
            try {
                std::string message = this->getMessage();
                if (!message.empty()) {
                    auto& world = api::World::instance();
                    // 受信したメッセージとして処理
                    world.onReceiveChat(message, *mod);
                }
            } catch (...) {
                // エラー時は無視
            }
        }
    }
    return res;
}

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

    // パケットフックを有効化
    TextPacketReceiveHook::hook();
    gChatHookMod.store(&mod, std::memory_order_relaxed);

    // PlayerChatEventリスナーを設定（ローカルワールド用）
    auto& eventBus = ll::event::EventBus::getInstance();

    chatListener = eventBus.emplaceListener<ll::event::PlayerChatEvent>(
        [&mod](ll::event::PlayerChatEvent& event) {
            auto& message = event.message();
            auto& player = event.self();

            mod.getSelf().getLogger().debug("Player chat: {} says '{}'",
                player.getRealName(), message);

            // コマンドチェック（プレフィックス '!' を使用）
            if (message.starts_with("!")) {
                auto& cmdManager = commands::CommandManager::getInstance();

                // コマンド文字列を解析（プレフィックスを除去）
                std::string cmdStr = message.substr(1);
                std::vector<std::string> parts;
                std::string current;

                // スペースでコマンドと引数を分割
                for (char c : cmdStr) {
                    if (c == ' ') {
                        if (!current.empty()) {
                            parts.push_back(current);
                            current.clear();
                        }
                    } else {
                        current += c;
                    }
                }
                if (!current.empty()) {
                    parts.push_back(current);
                }

                if (parts.empty()) {
                    return; // 空のコマンド
                }

                std::string cmdName = parts[0];
                std::vector<std::string> args(parts.begin() + 1, parts.end());

                mod.getSelf().getLogger().debug("Processing command: '{}' with {} args", cmdName, args.size());

                // コマンドを実行
                if (cmdManager.executeCommand(mod, cmdName, args)) {
                    // コマンドが実行された場合、チャットメッセージをキャンセル
                    event.cancel();
                    mod.getSelf().getLogger().debug("Command '{}' executed, chat message cancelled", cmdName);
                } else {
                    mod.getSelf().getLogger().debug("Unknown command: '{}'", cmdName);
                }
            }

            // ChatSendイベントをWorldクラスを通じてpublish
            auto& world = api::World::instance();
            world.onPlayerChat(player.getRealName(), message, mod);
        }
    );

    // LevelTickEventリスナーを設定
    tickListener = eventBus.emplaceListener<ll::event::LevelTickEvent>(
        [&mod](ll::event::LevelTickEvent& event) {
            // TickイベントをWorldクラスを通じてpublish
            auto& world = api::World::instance();
            world.onTick();
        }
    );

    if (chatListener && tickListener) {
        mod.getSelf().getLogger().info("Chat hook initialized successfully - {} commands available", commands.size());
        mod.getSelf().getLogger().debug("Event listeners and packet hooks registered");
    } else {
        mod.getSelf().getLogger().error("Failed to register event listeners");
    }
}

void shutdownChatHook() {
    // パケットフックを無効化
    gChatHookMod.store(nullptr, std::memory_order_relaxed);

    // イベントバスからリスナーを削除
    auto& eventBus = ll::event::EventBus::getInstance();

    if (chatListener) {
        eventBus.removeListener<ll::event::PlayerChatEvent>(chatListener);
        chatListener = nullptr;
    }

    if (tickListener) {
        eventBus.removeListener<ll::event::LevelTickEvent>(tickListener);
        tickListener = nullptr;
    }
}

} // namespace origin_mod::hooks


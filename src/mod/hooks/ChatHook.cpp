#include "mod/hooks/ChatHook.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include <fmt/format.h>
#include <sstream>

// LeviLamina event system includes (Tick event only)
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/world/LevelTickEvent.h"

// パケットフック用のインクルード
#include "ll/api/memory/Hook.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/network/packet/TextPacketPayload.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/network/packet/TextPacketType.h"

namespace origin_mod::hooks {

// グローバル変数
static ll::event::ListenerPtr tickListener = nullptr;
static std::atomic<OriginMod*> gChatHookMod{nullptr};

// ClientNetworkHandler::$handle TextPacketフック（受信処理）
LL_TYPE_INSTANCE_HOOK(
    ClientNetworkHandlerTextPacketHook,
    ll::memory::HookPriority::Highest,
    ClientNetworkHandler,
    &ClientNetworkHandler::$handle,
    void,
    ::NetworkIdentifier const& source,
    ::TextPacket const& packet
) {
    // 最優先でログ出力（他の処理より前）
    auto* hookMod = gChatHookMod.load(std::memory_order_relaxed);
    if (hookMod) {
        hookMod->getSelf().getLogger().info("=== ClientNetworkHandler TextPacket DETECTED ===");
    }

    // 元の処理を呼び出し
    origin(source, packet);

    // パケット処理後にカスタムイベントを発行
    auto* mod = gChatHookMod.load(std::memory_order_relaxed);
    if (mod) {
        try {
            // パケット処理とコマンド実行
            std::string message = packet.getMessage();
            std::string author = packet.getAuthorOrEmpty();

            // フラリアル方式: filteredMessageを優先的にチェック
            std::string actualMessage = message;
            // TODO: filteredMessageのアクセス方法を調査

            mod->getSelf().getLogger().debug("ClientNetworkHandler TextPacket received - Author: '{}', Message: '{}'",
                author.empty() ? "System" : author, message);

            // サーバーカスタムメッセージのデバッグ情報
            if (author.empty() && !message.empty()) {
                mod->getSelf().getLogger().info("Server custom message detected: '{}'", message);
            }

            // コマンド処理（Chat以外も対象に）
            if (!message.empty() && message[0] == '!') {
                // サーバーカスタムメッセージでもコマンドとして処理
                mod->getSelf().getLogger().info("Command detected in packet - Author: '{}', Message: '{}'",
                    author.empty() ? "System" : author, message);

                // コマンドの場合、CommandManagerで処理
                std::string commandLine = message.substr(1); // '!'を除去
                if (!commandLine.empty()) {
                    // コマンドと引数を分離
                    std::istringstream iss(commandLine);
                    std::string command;
                    iss >> command;

                    std::vector<std::string> args;
                    std::string arg;
                    while (iss >> arg) {
                        args.push_back(arg);
                    }

                    auto& cmdManager = commands::CommandManager::getInstance();
                    bool handled = cmdManager.executeCommand(*mod, command, args);

                    if (handled) {
                        mod->getSelf().getLogger().info("Command '{}' executed successfully via packet interception", command);
                        return; // コマンドが処理された場合は、以降の処理をスキップ
                    } else {
                        mod->getSelf().getLogger().debug("Unknown command '{}' via ClientNetworkHandler", command);
                    }
                }
            }

            auto& world = api::World::instance();

            // PacketReceiveイベントを発行
            world.onPacketReceive("TextPacket", message, author, *mod);

            // 既存のAPIとの後方互換性
            if (!message.empty()) {
                if (!author.empty()) {
                    world.onPlayerChat(author, message, *mod);
                } else {
                    world.onReceiveChat(message, *mod);
                }
            }

            mod->getSelf().getLogger().debug("Forwarded ClientNetworkHandler TextPacket to World events");
        } catch (const std::exception& e) {
            mod->getSelf().getLogger().debug("Exception in ClientNetworkHandler TextPacket hook: {}", e.what());
        } catch (...) {
            mod->getSelf().getLogger().debug("Unknown exception in ClientNetworkHandler TextPacket hook");
        }
    }
}

void initializeChatHook(OriginMod& mod) {
    mod.getSelf().getLogger().debug("Initializing pure packet-based chat hook (LeviLamina 1.9.5)...");

    // CommandManagerを初期化
    auto& cmdManager = commands::CommandManager::getInstance();
    cmdManager.initialize();

    auto commands = cmdManager.getCommands();
    mod.getSelf().getLogger().info("Chat commands registered: {} commands", commands.size());

    for (const auto& cmd : commands) {
        mod.getSelf().getLogger().debug("  - {}: {}", cmd.name, cmd.description);
    }

    // TextPacketフックを有効化（ClientNetworkHandlerのみ）
    try {
        ClientNetworkHandlerTextPacketHook::hook();
        mod.getSelf().getLogger().info("ClientNetworkHandler::$handle TextPacket hook registered successfully");
    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Failed to register ClientNetworkHandler TextPacket hook: {}", e.what());
    }

    gChatHookMod.store(&mod, std::memory_order_relaxed);
    mod.getSelf().getLogger().debug("Global mod pointer set for packet hooks");

    // LevelTickEventリスナーを設定
    auto& eventBus = ll::event::EventBus::getInstance();
    tickListener = eventBus.emplaceListener<ll::event::LevelTickEvent>(
        [&mod](ll::event::LevelTickEvent& event) {
            // TickイベントをWorldクラスを通じてpublish
            auto& world = api::World::instance();
            world.onTick();
        }
    );

    if (tickListener) {
        mod.getSelf().getLogger().info("Universal ClientNetworkHandler chat hook initialized successfully - {} commands available", commands.size());
        mod.getSelf().getLogger().info("Features: Universal TextPacket monitoring, Packet events, Server+Local command support");
        mod.getSelf().getLogger().info("Note: Commands work in both local worlds and servers via unified packet interception");
    } else {
        mod.getSelf().getLogger().error("Failed to register tick listener");
    }
}

void shutdownChatHook() {
    // パケットフックを無効化
    gChatHookMod.store(nullptr, std::memory_order_relaxed);

    // イベントバスからリスナーを削除
    if (tickListener) {
        auto& eventBus = ll::event::EventBus::getInstance();
        eventBus.removeListener<ll::event::LevelTickEvent>(tickListener);
        tickListener = nullptr;
    }
}

} // namespace origin_mod::hooks


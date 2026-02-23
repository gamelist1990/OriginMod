#include "mod/hooks/ChatHook.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/NetworkInfo.h"
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
#include "mc/network/MinecraftPacketIds.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/LoopbackPacketSender.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/network/packet/TextPacketType.h"

namespace origin_mod::hooks {

// グローバル変数
static ll::event::ListenerPtr tickListener = nullptr;
static std::atomic<OriginMod*> gChatHookMod{nullptr};

// LoopbackPacketSender::$sendToServer フック（送信処理 - フラリアル方式）
LL_TYPE_INSTANCE_HOOK(
    LoopbackPacketSenderHook,
    ll::memory::HookPriority::Highest,
    LoopbackPacketSender,
    &LoopbackPacketSender::$sendToServer,
    void,
    ::Packet& packet
) {
    auto* mod = gChatHookMod.load(std::memory_order_relaxed);
    if (mod) {
        try {
            if (packet.getId() == MinecraftPacketIds::Text) {
                auto* textPacket = static_cast<TextPacket*>(&packet);
                std::string message = textPacket->getMessage();
                // コマンド判定
                if (!message.empty() && message[0] == '-') {
                    // コマンドを処理
                    std::string commandLine = message.substr(1); 
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
                        cmdManager.executeCommand(*mod, command, args);
                    }

                    return;
                }
            }
        } catch (const std::exception& e) {
            mod->getSelf().getLogger().error("Exception in sendToServer hook: {}", e.what());
        } catch (...) {
            mod->getSelf().getLogger().error("Unknown exception in sendToServer hook");
        }
    }
    origin(packet);
}

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
    auto* hookMod = gChatHookMod.load(std::memory_order_relaxed);
    origin(source, packet);
    auto* mod = gChatHookMod.load(std::memory_order_relaxed);
    if (mod) {
        try {
            // Best-effort: cache server endpoint for -ping command.
            try {
                auto ipPort = source.getIPAndPort();
                if (!ipPort.empty()) {
                    origin_mod::api::NetworkInfo::setServerIpPort(ipPort);
                }
            } catch (...) {
            }

            // パケット処理とコマンド実行
            std::string message = packet.getMessage();
            std::string author = packet.getAuthorOrEmpty();
            std::string actualMessage = message;
            mod->getSelf().getLogger().debug("ClientNetworkHandler TextPacket received - Author: '{}', Message: '{}'",
                author.empty() ? "System" : author, message);

            // サーバーカスタムメッセージのデバッグ情報
            if (author.empty() && !message.empty()) {
                mod->getSelf().getLogger().info("Server custom message detected: '{}'", message);
            }
            auto& world = api::World::instance();
            world.onPacketReceive("TextPacket", message, author, *mod);
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

    // TextPacketフックを有効化（受信側）
    try {
        ClientNetworkHandlerTextPacketHook::hook();
        mod.getSelf().getLogger().info("ClientNetworkHandler::$handle TextPacket hook registered successfully");
    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Failed to register ClientNetworkHandler TextPacket hook: {}", e.what());
    }

    // パケット送信フックを有効化（送信側 - フラリアル方式）
    try {
        LoopbackPacketSenderHook::hook();
        mod.getSelf().getLogger().info("LoopbackPacketSender::$sendToServer hook registered successfully");
    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Failed to register LoopbackPacketSender hook: {}", e.what());
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


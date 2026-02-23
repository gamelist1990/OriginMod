#include "mod/hooks/ChatHook.h"
#include "mod/OriginMod.h"
#include "mod/commands/CommandManager.h"
#include "mod/api/Player.h"
#include "mod/api/World.h"
#include <fmt/format.h>

// LeviLamina includes
#include "ll/api/memory/Hook.h"
#include "ll/api/event/EventBus.h"
#include "ll/api/event/Listener.h"
#include "ll/api/event/world/LevelTickEvent.h"
#include "mc/network/packet/TextPacket.h"
#include "mc/client/network/ClientNetworkHandler.h"
#include "mc/network/NetworkIdentifier.h"
#include "mc/network/packet/TextPacketType.h"
#include "mc/network/PacketSender.h"
#include "mc/client/game/ClientInstance.h"
#include "mc/client/player/LocalPlayer.h"

namespace origin_mod::hooks {

static std::atomic<OriginMod*> gChatHookMod{nullptr};
static ll::event::ListenerPtr tickListener = nullptr;

// PacketSender::sendToServer フック（送信前コマンド処理）
LL_TYPE_INSTANCE_HOOK(
    PacketSenderSendToServerHook,
    ll::memory::HookPriority::High,
    PacketSender,
    &PacketSender::sendToServer,
    void,
    ::Packet& packet
) {
    auto* mod = gChatHookMod.load(std::memory_order_relaxed);

    // TextPacketかチェック
    if (packet.getId() == MinecraftPacketIds::Text) {
        auto* textPacket = reinterpret_cast<TextPacket*>(&packet);

        try {
            // TextPacketPayload構造体からメッセージとタイプを取得
            auto& payload = textPacket->mBody;

            // std::variantから適切な型を取得
            if (std::holds_alternative<TextPacketPayload::MessageOnly>(payload)) {
                auto& msgOnly = std::get<TextPacketPayload::MessageOnly>(payload);

                if (msgOnly.mType == TextPacketType::Chat) {
                    std::string message = msgOnly.mMessage;

                    if (mod) {
                        mod->getSelf().getLogger().debug("Outgoing chat message: '{}'", message);

                        // コマンドチェック（プレフィックス '!' を使用）
                        if (message.starts_with("!")) {
                            mod->getSelf().getLogger().debug("Command detected: '{}'", message);

                            auto& cmdManager = commands::CommandManager::getInstance();

                            // コマンド解析
                            std::string cmdStr = message.substr(1);
                            std::vector<std::string> parts;
                            std::string current;

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

                            if (!parts.empty()) {
                                std::string cmdName = parts[0];
                                std::vector<std::string> args(parts.begin() + 1, parts.end());

                                // コマンドを実行
                                if (cmdManager.executeCommand(*mod, cmdName, args)) {
                                    mod->getSelf().getLogger().debug("Command '{}' executed, packet cancelled", cmdName);
                                    // コマンドが成功した場合、パケット送信をキャンセル
                                    return;
                                } else {
                                    mod->getSelf().getLogger().debug("Unknown command: '{}'", cmdName);
                                }
                            }
                        }

                        // 通常のチャットメッセージとしてWorld APIに通知
                        auto& world = api::World::instance();
                        if (auto player = mc::ClientInstance::get().getLocalPlayer()) {
                            world.onPlayerChat(player->getPlayerName(), message, *mod);
                        }
                    }
                }
            }
        } catch (const std::exception& e) {
            if (mod) {
                mod->getSelf().getLogger().debug("Exception in sendToServer hook: {}", e.what());
            }
        }
    }

    // 元の送信処理を実行
    origin(packet);
}

// ClientNetworkHandler::handle TextPacket フック（受信処理）
LL_TYPE_INSTANCE_HOOK(
    ClientNetworkHandlerTextPacketHook,
    ll::memory::HookPriority::Normal,
    ClientNetworkHandler,
    &ClientNetworkHandler::handle,
    void,
    ::NetworkIdentifier const& source,
    ::TextPacket const& packet
) {
    // 元の処理を先に実行
    origin(source, packet);

    auto* mod = gChatHookMod.load(std::memory_order_relaxed);
    if (mod) {
        try {
            auto& payload = packet.mBody;

            std::string message;
            std::string senderName;
            TextPacketType packetType = TextPacketType::Raw;

            // std::variantから適切な値を取得
            if (std::holds_alternative<TextPacketPayload::MessageOnly>(payload)) {
                auto& msgOnly = std::get<TextPacketPayload::MessageOnly>(payload);
                message = msgOnly.mMessage;
                packetType = msgOnly.mType;
            }
            else if (std::holds_alternative<TextPacketPayload::AuthorAndMessage>(payload)) {
                auto& authorMsg = std::get<TextPacketPayload::AuthorAndMessage>(payload);
                message = authorMsg.mMessage;
                senderName = authorMsg.mAuthor;
                packetType = authorMsg.mType;
            }
            else if (std::holds_alternative<TextPacketPayload::MessageAndParams>(payload)) {
                auto& msgParams = std::get<TextPacketPayload::MessageAndParams>(payload);
                message = msgParams.mMessage;
                packetType = msgParams.mType;
            }

            if (!message.empty()) {
                // パケットタイプを文字列に変換
                std::string packetTypeStr;
                switch (packetType) {
                    case TextPacketType::Chat: packetTypeStr = "Chat"; break;
                    case TextPacketType::SystemMessage: packetTypeStr = "System"; break;
                    case TextPacketType::Whisper: packetTypeStr = "Whisper"; break;
                    case TextPacketType::Announcement: packetTypeStr = "Announcement"; break;
                    default: packetTypeStr = "Other"; break;
                }

                mod->getSelf().getLogger().debug("Received {} message from '{}': '{}'",
                    packetTypeStr, senderName.empty() ? "System" : senderName, message);

                auto& world = api::World::instance();

                // PacketReceiveイベントを発行
                world.onPacketReceive(packetTypeStr, message, senderName, *mod);

                // 既存のメソッドも呼び出し（後方互換性）
                if (packetType == TextPacketType::Chat && !senderName.empty()) {
                    world.onPlayerChat(senderName, message, *mod);
                } else {
                    world.onReceiveChat(message, *mod);
                }
            }
        } catch (const std::exception& e) {
            mod->getSelf().getLogger().debug("Exception in TextPacket receive hook: {}", e.what());
        }
    }
}

void initializeChatHook(OriginMod& mod) {
    mod.getSelf().getLogger().debug("Initializing improved chat hook (LeviLamina 1.9.5)...");

    // CommandManagerを初期化
    auto& cmdManager = commands::CommandManager::getInstance();
    cmdManager.initialize();

    auto commands = cmdManager.getCommands();
    mod.getSelf().getLogger().info("Chat commands registered: {} commands", commands.size());

    for (const auto& cmd : commands) {
        mod.getSelf().getLogger().debug("  - {}: {}", cmd.name, cmd.description);
    }

    // パケットフックを有効化
    try {
        PacketSenderSendToServerHook::hook();
        mod.getSelf().getLogger().info("PacketSender::sendToServer hook registered successfully");

        ClientNetworkHandlerTextPacketHook::hook();
        mod.getSelf().getLogger().info("ClientNetworkHandler::handle TextPacket hook registered successfully");

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

        mod.getSelf().getLogger().info("Improved chat hook initialized successfully - {} commands available", commands.size());
        mod.getSelf().getLogger().info("Features: Server/Local unified, Command cancellation, Packet events");
    } catch (const std::exception& e) {
        mod.getSelf().getLogger().error("Failed to register packet hooks: {}", e.what());
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

    // フックの解除は自動的に行われる
}

} // namespace origin_mod::hooks


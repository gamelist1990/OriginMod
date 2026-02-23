#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <sstream>
#include <climits>

#include "mod/api/Player.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::commands {

class OriginCommandRegistry {
public:
    struct CommandMeta {
        std::string name;        // subcommand name (e.g. "help", "toggle")
        std::string description; // one-liner
    };

    // Reply function used by handlers to send back response text.
    using ReplyFn = std::function<void(std::string const&)>;

    // Handler for internal dispatch (chat-based). Arguments are the tokens after the subcommand name.
    using HandlerFn = std::function<void(origin_mod::OriginMod&, std::vector<std::string> const& args, ReplyFn)>;

    class Builder {
    public:
        Builder(OriginCommandRegistry& reg, std::string name);

        Builder& description(std::string desc);

        // Finalize registration by providing the handler for this subcommand.
        [[nodiscard]] bool then(HandlerFn fn);

    private:
        OriginCommandRegistry& mReg;
        std::string            mName;
        std::string            mDesc;
    };

    [[nodiscard]] Builder registerCommand(std::string_view name);

    // Developer-friendly descriptor registration.
    struct Descriptor {
        std::string name;
        std::string description;
        int minArgs{0};
        int maxArgs{INT_MAX};

        struct Context {
            origin_mod::OriginMod&              mod;
            origin_mod::api::Player&        player;
            OriginCommandRegistry const& registry;
        };

        // Executor receives a Bukkit-like Player wrapper + registry access.
        std::function<void(Context const& ctx, std::vector<std::string> const& args)> executor;
    };

    [[nodiscard]] bool registerCommand(Descriptor desc);

    [[nodiscard]] std::vector<CommandMeta> list() const;
    [[nodiscard]] std::optional<CommandMeta> get(std::string_view name) const;

    // Execute a registered subcommand handler. Returns true if handled.
    bool execute(origin_mod::OriginMod& mod, std::string_view name, std::vector<std::string> const& args, ReplyFn reply) const;

private:
    struct Entry {
        CommandMeta meta;
        HandlerFn   fn;
    };

    [[nodiscard]] static std::string toLower(std::string_view s);

    [[nodiscard]] bool registerImpl(std::string nameLower, std::string desc, HandlerFn fn);

    // key: lowercase name
    std::unordered_map<std::string, Entry> mEntries;
};

} // namespace origin_mod::commands

#include "mod/commands/OriginCommandRegistry.h"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <climits>

namespace origin_mod::commands {

static std::string normalizeLookupKey(std::string_view name) {
    std::string s{name};

    auto trimAscii = [](std::string& x) {
        while (!x.empty()) {
            unsigned char c = static_cast<unsigned char>(x.front());
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
                x.erase(x.begin());
                continue;
            }
            break;
        }
        while (!x.empty()) {
            unsigned char c = static_cast<unsigned char>(x.back());
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f') {
                x.pop_back();
                continue;
            }
            break;
        }
    };

    auto trimFullWidthSpace = [](std::string& x) {
        static constexpr std::string_view kFw = "\xE3\x80\x80"; // U+3000
        while (x.size() >= kFw.size() && std::string_view{x}.starts_with(kFw)) {
            x.erase(0, kFw.size());
        }
        while (x.size() >= kFw.size() && std::string_view{x}.ends_with(kFw)) {
            x.erase(x.size() - kFw.size());
        }
    };

    trimAscii(s);
    trimFullWidthSpace(s);

    // Accept prefixed forms.
    while (!s.empty() && (s.front() == '-' || s.front() == '/' || s.front() == '.')) {
        s.erase(s.begin());
    }

    trimAscii(s);
    trimFullWidthSpace(s);

    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

OriginCommandRegistry::Builder::Builder(OriginCommandRegistry& reg, std::string name)
: mReg(reg),
  mName(std::move(name)) {}

OriginCommandRegistry::Builder& OriginCommandRegistry::Builder::description(std::string desc) {
    mDesc = std::move(desc);
    return *this;
}

bool OriginCommandRegistry::Builder::then(HandlerFn fn) {
    if (!fn) {
        return false;
    }
    auto key = OriginCommandRegistry::toLower(mName);
    return mReg.registerImpl(std::move(key), std::move(mDesc), std::move(fn));
}

OriginCommandRegistry::Builder OriginCommandRegistry::registerCommand(std::string_view name) {
    return Builder{*this, std::string{name}};
}

std::vector<OriginCommandRegistry::CommandMeta> OriginCommandRegistry::list() const {
    std::vector<CommandMeta> out;
    out.reserve(mEntries.size());
    for (auto const& [_, e] : mEntries) {
        out.push_back(e.meta);
    }
    std::ranges::sort(out, [](auto const& a, auto const& b) { return a.name < b.name; });
    return out;
}

std::optional<OriginCommandRegistry::CommandMeta> OriginCommandRegistry::get(std::string_view name) const {
    auto key = normalizeLookupKey(name);
    auto it  = mEntries.find(key);
    if (it == mEntries.end()) {
        return std::nullopt;
    }
    return it->second.meta;
}

bool OriginCommandRegistry::execute(origin_mod::OriginMod& mod, std::string_view name, std::vector<std::string> const& args, ReplyFn reply) const {
    auto key = normalizeLookupKey(name);
    auto it  = mEntries.find(key);
    if (it == mEntries.end()) {
        return false;
    }
    auto const& entry = it->second;
    if (entry.fn) {
        entry.fn(mod, args, std::move(reply));
        return true;
    }
    return false;
}

std::string OriginCommandRegistry::toLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool OriginCommandRegistry::registerImpl(std::string nameLower, std::string desc, HandlerFn fn) {
    if (nameLower.empty() || !fn) {
        return false;
    }
    Entry e{
        .meta = CommandMeta{.name = nameLower, .description = std::move(desc)},
        .fn   = std::move(fn),
    };
    return mEntries.emplace(std::move(nameLower), std::move(e)).second;
}

bool OriginCommandRegistry::registerCommand(OriginCommandRegistry::Descriptor desc) {
    if (desc.name.empty() || !desc.executor) return false;
    // Wrap executor into HandlerFn
    OriginCommandRegistry const* self = this;
    HandlerFn h = [d = std::move(desc), self](origin_mod::OriginMod& mod, std::vector<std::string> const& args, ReplyFn reply) mutable {
        if ((int)args.size() < d.minArgs) {
            reply("§c[!] 引数が足りません。");
            return;
        }
        if ((int)args.size() > d.maxArgs) {
            reply("§c[!] 引数が多すぎます。");
            return;
        }

        // Descriptor executors are developer-facing and should reply locally by default.
        // (Developers can still call ctx.player.sendMessage() to send to server.)
        origin_mod::api::Player player{mod};
        Descriptor::Context ctx{.mod = mod, .player = player, .registry = *self};
        d.executor(ctx, args);
    };

    auto key = toLower(desc.name);
    return registerImpl(std::move(key), std::move(desc.description), std::move(h));
}

} // namespace origin_mod::commands

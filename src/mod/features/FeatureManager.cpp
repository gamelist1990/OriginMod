#include "mod/features/FeatureManager.h"

#include <algorithm>
#include <cctype>
#include <fstream>

#include <nlohmann/json.hpp>

#include "ll/api/event/EventBus.h"
#include "ll/api/event/world/LevelTickEvent.h"

#include "mod/OriginMod.h"
#include "mod/config/ConfigManager.h"
#include "mod/features/FeatureRegistrar.h"
#include "mod/features/FeatureRegistry.h"
#include "mod/features/IFeature.h"

namespace origin_mod::features {

using json = nlohmann::json;

FeatureManager& FeatureManager::instance() {
    static FeatureManager g;
    return g;
}

void FeatureManager::initialize(origin_mod::OriginMod& mod) {
    if (mInitialized) {
        return;
    }

    // Collect registered features into registry.
    FeatureRegistry& reg = FeatureRegistry::instance();
    FeatureRegistrar::instance().applyAll(reg);

    loadAllFeatures();
    loadConfig(mod);
    applyEnabledStates(mod);

    // Tick listener
    mTickListener = ll::event::EventBus::getInstance().emplaceListener<ll::event::world::LevelTickEvent>(
        [&mod](ll::event::world::LevelTickEvent&) { FeatureManager::instance().onTick(mod); },
        ll::event::EventPriority::Low
    );
    if (!mTickListener) {
        mod.getSelf().getLogger().warn("Feature tick listener registration failed");
    }

    mInitialized = true;
    mod.getSelf().getLogger().debug("Features initialized: {}", mFeatures.size());
}

void FeatureManager::shutdown(origin_mod::OriginMod& mod) {
    if (!mInitialized) {
        return;
    }

    // Disable all enabled features.
    for (auto& f : mFeatures) {
        auto id = toLower(f->id());
        if (mEnabled[id]) {
            f->onDisable(mod);
            mEnabled[id] = false;
        }
    }
    saveConfig(mod);

    if (mTickListener) {
        ll::event::EventBus::getInstance().removeListener<ll::event::world::LevelTickEvent>(mTickListener);
        mTickListener.reset();
    }

    mById.clear();
    mFeatures.clear();
    mEnabled.clear();
    mInitialized = false;
}

std::vector<FeatureInfo> FeatureManager::list() const {
    std::vector<FeatureInfo> out;
    out.reserve(mFeatures.size());
    for (auto const& f : mFeatures) {
        auto id = toLower(f->id());
        out.push_back(FeatureInfo{
            .id          = id,
            .name        = std::string{f->name()},
            .description = std::string{f->description()},
            .enabled     = mEnabled.contains(id) ? mEnabled.at(id) : false,
        });
    }
    std::ranges::sort(out, [](FeatureInfo const& a, FeatureInfo const& b) { return a.id < b.id; });
    return out;
}

std::optional<FeatureInfo> FeatureManager::get(std::string_view id) const {
    auto key = toLower(id);
    auto it  = mById.find(key);
    if (it == mById.end()) {
        return std::nullopt;
    }
    auto* f = it->second;
    return FeatureInfo{
        .id          = key,
        .name        = std::string{f->name()},
        .description = std::string{f->description()},
        .enabled     = mEnabled.contains(key) ? mEnabled.at(key) : false,
    };
}

std::optional<bool> FeatureManager::setEnabled(
    origin_mod::OriginMod& mod,
    std::string_view id,
    std::optional<bool> desired
) {
    if (!mInitialized) {
        initialize(mod);
    }

    auto key = toLower(id);
    auto it  = mById.find(key);
    if (it == mById.end()) {
        return std::nullopt;
    }
    auto* f = it->second;

    bool current = mEnabled.contains(key) ? mEnabled[key] : false;
    bool next    = desired.has_value() ? *desired : !current;
    if (next == current) {
        return current;
    }

    if (next) {
        f->onEnable(mod);
    } else {
        f->onDisable(mod);
    }
    mEnabled[key] = next;
    saveConfig(mod);
    return next;
}

std::string FeatureManager::toLower(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

void FeatureManager::loadAllFeatures() {
    mFeatures = FeatureRegistry::instance().createAll();
    mById.clear();
    mById.reserve(mFeatures.size());
    for (auto& f : mFeatures) {
        auto id = toLower(f->id());
        mById.emplace(id, f.get());
    }
}

void FeatureManager::loadConfig(origin_mod::OriginMod& mod) {
    mEnabled.clear();

    // ConfigManagerを使用して設定を読み込み
    auto& configManager = origin_mod::config::ConfigManager::instance();
    auto configOpt = configManager.loadConfig("features.json");

    if (!configOpt.has_value()) {
        return;
    }

    json config = configOpt.value();
    if (!config.is_object()) {
        return;
    }

    auto it = config.find("features");
    if (it == config.end() || !it->is_object()) {
        return;
    }

    for (auto const& [k, v] : it->items()) {
        if (v.is_boolean()) {
            mEnabled[toLower(k)] = v.get<bool>();
        }
    }
}

void FeatureManager::saveConfig(origin_mod::OriginMod& mod) const {
    // ConfigManagerを使用して設定を保存
    json j;
    j["features"] = json::object();
    for (auto const& [k, v] : mEnabled) {
        j["features"][k] = v;
    }

    auto& configManager = origin_mod::config::ConfigManager::instance();
    configManager.saveConfig("features.json", j);
}

void FeatureManager::applyEnabledStates(origin_mod::OriginMod& mod) {
    // Ensure every feature has a key (default false).
    for (auto const& f : mFeatures) {
        auto id = toLower(f->id());
        if (!mEnabled.contains(id)) {
            mEnabled[id] = false;
        }
    }
    // Enable those marked true.
    for (auto& f : mFeatures) {
        auto id = toLower(f->id());
        if (mEnabled[id]) {
            f->onEnable(mod);
        }
    }
}

void FeatureManager::onTick(origin_mod::OriginMod& mod) {
    // Call enabled features.
    for (auto& f : mFeatures) {
        auto id = toLower(f->id());
        if (mEnabled[id]) {
            f->onTick(mod);
        }
    }
}

void FeatureManager::reloadConfig(origin_mod::OriginMod& mod) {
    if (!mInitialized) {
        return;
    }

    // 現在の状態を保存してからリロード
    auto oldStates = mEnabled;
    loadConfig(mod);

    // 変更された機能の状態を適用
    for (auto& f : mFeatures) {
        auto id = toLower(f->id());
        bool oldEnabled = oldStates.contains(id) ? oldStates[id] : false;
        bool newEnabled = mEnabled.contains(id) ? mEnabled[id] : false;

        if (oldEnabled != newEnabled) {
            if (newEnabled) {
                f->onEnable(mod);
            } else {
                f->onDisable(mod);
            }
        }
    }
}

std::vector<std::string> FeatureManager::featureIds() const {
    std::vector<std::string> out;
    out.reserve(mFeatures.size());
    for (auto const& f : mFeatures) {
        out.emplace_back(toLower(f->id()));
    }
    return out;
}

} // namespace origin_mod::features

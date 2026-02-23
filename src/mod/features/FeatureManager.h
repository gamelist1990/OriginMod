#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "ll/api/event/ListenerBase.h"

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::features {

class IFeature;

struct FeatureInfo {
    std::string id;
    std::string name;
    std::string description;
    bool        enabled{};
};

class FeatureManager {
public:
    static FeatureManager& instance();

    // Idempotent. Safe to call from commands.
    void initialize(origin_mod::OriginMod& mod);
    void shutdown(origin_mod::OriginMod& mod);

    [[nodiscard]] std::vector<FeatureInfo> list() const;
    [[nodiscard]] std::optional<FeatureInfo> get(std::string_view id) const;

    // If desired is not set, toggle.
    // Returns new state, or nullopt if unknown feature.
    [[nodiscard]] std::optional<bool> setEnabled(origin_mod::OriginMod& mod, std::string_view id, std::optional<bool> desired);

    [[nodiscard]] bool isInitialized() const noexcept { return mInitialized; }

    // Returns all feature IDs as strings (for command soft-enum registration).
    [[nodiscard]] std::vector<std::string> featureIds() const;

    // Reload feature configuration from ConfigManager
    void reloadConfig(origin_mod::OriginMod& mod);

private:
    FeatureManager() = default;

    [[nodiscard]] static std::string toLower(std::string_view s);

    void loadAllFeatures();
    void loadConfig(origin_mod::OriginMod& mod);
    void saveConfig(origin_mod::OriginMod& mod) const;

    void applyEnabledStates(origin_mod::OriginMod& mod);

    void onTick(origin_mod::OriginMod& mod);

    bool mInitialized{false};

    std::vector<std::unique_ptr<IFeature>> mFeatures;
    std::unordered_map<std::string, IFeature*> mById;
    std::unordered_map<std::string, bool> mEnabled;

    ll::event::ListenerPtr mTickListener;
};

} // namespace origin_mod::features

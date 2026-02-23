#include "mod/config/ConfigManager.h"
#include "mod/OriginMod.h"

#include <fstream>
#include <filesystem>

using json = nlohmann::json;

namespace origin_mod::config {

ConfigManager& ConfigManager::instance() {
    static ConfigManager g;
    return g;
}

void ConfigManager::initialize(origin_mod::OriginMod& mod) {
    if (mInitialized) return;

    mMod = &mod;
    mConfigDir = mod.getSelf().getConfigDir();

    createConfigDirectoryIfNeeded();

    mInitialized = true;

    mod.getSelf().getLogger().info("ConfigManager initialized - config directory: {}",
        mConfigDir.string());
}

void ConfigManager::shutdown() {
    if (!mInitialized) return;

    mConfigCache.clear();
    mMod = nullptr;
    mInitialized = false;
}

std::optional<json> ConfigManager::loadConfig(const std::string& fileName) {
    if (!mInitialized) return std::nullopt;

    // キャッシュから取得を試行
    auto it = mConfigCache.find(fileName);
    if (it != mConfigCache.end()) {
        return it->second;
    }

    // ファイルから読み込み
    if (loadConfigInternal(fileName)) {
        return mConfigCache[fileName];
    }

    return std::nullopt;
}

bool ConfigManager::saveConfig(const std::string& fileName, const json& data) {
    if (!mInitialized) return false;

    auto filePath = getConfigPath(fileName);

    try {
        std::ofstream ofs(filePath);
        if (!ofs.is_open()) {
            if (mMod) {
                mMod->getSelf().getLogger().warn("Failed to open config file for writing: {}",
                    filePath.string());
            }
            return false;
        }

        ofs << data.dump(2); // 2スペースインデント
        ofs.close();

        // キャッシュを更新
        mConfigCache[fileName] = data;

        if (mMod) {
            mMod->getSelf().getLogger().debug("Config saved: {}", fileName);
        }
        return true;

    } catch (const std::exception& e) {
        if (mMod) {
            mMod->getSelf().getLogger().error("Failed to save config {}: {}", fileName, e.what());
        }
        return false;
    }
}

void ConfigManager::reloadConfig(const std::string& fileName) {
    if (!mInitialized) return;

    // キャッシュから削除
    mConfigCache.erase(fileName);

    // 再読み込み
    loadConfigInternal(fileName);
}

void ConfigManager::reloadAllConfigs() {
    if (!mInitialized) return;

    // 全キャッシュをクリア
    auto fileNames = std::vector<std::string>{};
    for (const auto& [name, _] : mConfigCache) {
        fileNames.push_back(name);
    }

    mConfigCache.clear();

    // 再読み込み
    for (const auto& fileName : fileNames) {
        loadConfigInternal(fileName);
    }

    if (mMod) {
        mMod->getSelf().getLogger().info("All configs reloaded ({} files)", fileNames.size());
    }
}

void ConfigManager::ensureDefaultConfig(const std::string& fileName, const json& defaultData) {
    if (!mInitialized) return;

    auto filePath = getConfigPath(fileName);

    // ファイルが存在しない場合のみデフォルト値で作成
    if (!std::filesystem::exists(filePath)) {
        saveConfig(fileName, defaultData);

        if (mMod) {
            mMod->getSelf().getLogger().info("Created default config file: {}", fileName);
        }
    }
}

std::filesystem::path ConfigManager::getConfigDir() const {
    return mConfigDir;
}

std::filesystem::path ConfigManager::getConfigPath(const std::string& fileName) const {
    return mConfigDir / fileName;
}

bool ConfigManager::loadConfigInternal(const std::string& fileName) {
    auto filePath = getConfigPath(fileName);

    if (!std::filesystem::exists(filePath)) {
        if (mMod) {
            mMod->getSelf().getLogger().debug("Config file not found: {}", fileName);
        }
        return false;
    }

    try {
        std::ifstream ifs(filePath);
        if (!ifs.is_open()) {
            if (mMod) {
                mMod->getSelf().getLogger().warn("Failed to open config file: {}", fileName);
            }
            return false;
        }

        json data;
        ifs >> data;
        ifs.close();

        mConfigCache[fileName] = data;

        if (mMod) {
            mMod->getSelf().getLogger().debug("Config loaded: {}", fileName);
        }
        return true;

    } catch (const std::exception& e) {
        if (mMod) {
            mMod->getSelf().getLogger().error("Failed to parse config {}: {}", fileName, e.what());
        }
        return false;
    }
}

void ConfigManager::createConfigDirectoryIfNeeded() {
    if (!std::filesystem::exists(mConfigDir)) {
        try {
            std::filesystem::create_directories(mConfigDir);

            if (mMod) {
                mMod->getSelf().getLogger().info("Created config directory: {}", mConfigDir.string());
            }
        } catch (const std::exception& e) {
            if (mMod) {
                mMod->getSelf().getLogger().error("Failed to create config directory: {}", e.what());
            }
        }
    }
}

} // namespace origin_mod::config
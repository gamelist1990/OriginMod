#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace origin_mod {
class OriginMod;
}

namespace origin_mod::config {

/**
 * 統一設定マネージャー
 * 全ての設定ファイルの読み込み・管理を統一化
 */
class ConfigManager {
public:
    static ConfigManager& instance();

    // 初期化・終了処理
    void initialize(origin_mod::OriginMod& mod);
    void shutdown();

    /**
     * 設定ファイルの読み込み
     * @param fileName 設定ファイル名 (例: "messages.json")
     * @return 読み込まれたJSON、失敗時はnullopt
     */
    std::optional<nlohmann::json> loadConfig(const std::string& fileName);

    /**
     * 設定ファイルの保存
     * @param fileName 設定ファイル名
     * @param data 保存するJSONデータ
     * @return 成功時true
     */
    bool saveConfig(const std::string& fileName, const nlohmann::json& data);

    /**
     * 設定ファイルのリロード
     * キャッシュをクリアして再読み込み
     */
    void reloadConfig(const std::string& fileName);

    /**
     * 全設定ファイルのリロード
     */
    void reloadAllConfigs();

    /**
     * デフォルト設定ファイルの作成
     * ファイルが存在しない場合にデフォルト値で作成
     */
    void ensureDefaultConfig(const std::string& fileName, const nlohmann::json& defaultData);

    /**
     * 設定ディレクトリのパスを取得
     */
    [[nodiscard]] std::filesystem::path getConfigDir() const;

    /**
     * 特定の設定ファイルの絶対パスを取得
     */
    [[nodiscard]] std::filesystem::path getConfigPath(const std::string& fileName) const;

    [[nodiscard]] bool isInitialized() const noexcept { return mInitialized; }

private:
    ConfigManager() = default;

    // 設定キャッシュ
    std::unordered_map<std::string, nlohmann::json> mConfigCache;

    // OriginModインスタンスの参照
    origin_mod::OriginMod* mMod{nullptr};
    bool mInitialized{false};

    // 設定ディレクトリのパス
    std::filesystem::path mConfigDir;

    // 内部ヘルパーメソッド
    bool loadConfigInternal(const std::string& fileName);
    void createConfigDirectoryIfNeeded();
};

} // namespace origin_mod::config
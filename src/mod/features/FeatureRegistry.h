#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mod/features/IFeature.h"

namespace origin_mod::features {

class FeatureRegistry {
public:
    using Factory = std::function<std::unique_ptr<IFeature>()>;

    static FeatureRegistry& instance();

    [[nodiscard]] bool registerFeature(std::string id, Factory factory);

    template <class T>
        requires std::derived_from<T, IFeature>
    [[nodiscard]] bool registerFeature(std::string id) {
        return registerFeature(std::move(id), [] { return std::make_unique<T>(); });
    }

    [[nodiscard]] std::vector<std::string_view> ids() const;
    [[nodiscard]] std::vector<std::unique_ptr<IFeature>> createAll() const;

private:
    FeatureRegistry() = default;

    std::vector<std::pair<std::string, Factory>> mFactories;
};

} // namespace origin_mod::features

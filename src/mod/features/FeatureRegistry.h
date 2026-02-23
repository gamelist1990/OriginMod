#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <type_traits>

#include "mod/features/IFeature.h"

namespace origin_mod::features {

class FeatureRegistry {
public:
    using Factory = std::function<std::unique_ptr<IFeature>()>;

    static FeatureRegistry& instance() {
        static FeatureRegistry inst;
        return inst;
    }

    // Register a feature type with a stable id.  The caller is responsible for
    // ensuring the id is unique; duplicates will simply result in multiple
    // entries returned by `createAll`.
    template <typename F>
    void registerFeature(std::string_view id) {
        static_assert(std::is_base_of<IFeature, F>::value, "Feature must derive from IFeature");
        mFactories.emplace_back(std::string(id), []() {
            return std::make_unique<F>();
        });
    }

    // Instantiate one object for each registered feature.  Ownership is passed
    // to the caller.
    std::vector<std::unique_ptr<IFeature>> createAll() const {
        std::vector<std::unique_ptr<IFeature>> out;
        out.reserve(mFactories.size());
        for (auto const& [id, factory] : mFactories) {
            out.push_back(factory());
        }
        return out;
    }

private:
    FeatureRegistry() = default;
    std::vector<std::pair<std::string, Factory>> mFactories;
};

} // namespace origin_mod::features

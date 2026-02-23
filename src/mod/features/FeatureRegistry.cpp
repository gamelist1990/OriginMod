#include "mod/features/FeatureRegistry.h"

#include <algorithm>

namespace origin_mod::features {

FeatureRegistry& FeatureRegistry::instance() {
    static FeatureRegistry g;
    return g;
}

bool FeatureRegistry::registerFeature(std::string id, Factory factory) {
    if (id.empty() || !factory) {
        return false;
    }
    auto it = std::ranges::find_if(mFactories, [&](auto const& p) { return p.first == id; });
    if (it != mFactories.end()) {
        return false;
    }
    mFactories.emplace_back(std::move(id), std::move(factory));
    return true;
}

std::vector<std::string_view> FeatureRegistry::ids() const {
    std::vector<std::string_view> out;
    out.reserve(mFactories.size());
    for (auto const& [id, _] : mFactories) {
        out.emplace_back(id);
    }
    return out;
}

std::vector<std::unique_ptr<IFeature>> FeatureRegistry::createAll() const {
    std::vector<std::unique_ptr<IFeature>> out;
    out.reserve(mFactories.size());
    for (auto const& [_, factory] : mFactories) {
        if (auto f = factory()) {
            out.emplace_back(std::move(f));
        }
    }
    return out;
}

} // namespace origin_mod::features

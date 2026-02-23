#include "mod/modules/ModuleRegistry.h"

#include <algorithm>

namespace origin_mod::modules {

ModuleRegistry& ModuleRegistry::instance() {
    static ModuleRegistry gInstance;
    return gInstance;
}

bool ModuleRegistry::registerModule(std::string id, Factory factory) {
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

std::vector<std::string_view> ModuleRegistry::ids() const {
    std::vector<std::string_view> out;
    out.reserve(mFactories.size());
    for (auto const& [id, _] : mFactories) {
        out.emplace_back(id);
    }
    return out;
}

std::vector<std::unique_ptr<IModule>> ModuleRegistry::createAll() const {
    std::vector<std::unique_ptr<IModule>> out;
    out.reserve(mFactories.size());
    for (auto const& [_, factory] : mFactories) {
        if (auto mod = factory()) {
            out.emplace_back(std::move(mod));
        }
    }
    return out;
}

} // namespace origin_mod::modules

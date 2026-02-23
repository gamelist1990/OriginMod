#pragma once

#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mod/modules/IModule.h"

namespace origin_mod::modules {

class ModuleRegistry {
public:
    using Factory = std::function<std::unique_ptr<IModule>()>;

    static ModuleRegistry& instance();

    [[nodiscard]] bool registerModule(std::string id, Factory factory);

    template <class T>
        requires std::derived_from<T, IModule>
    [[nodiscard]] bool registerModule(std::string id) {
        return registerModule(std::move(id), [] { return std::make_unique<T>(); });
    }

    [[nodiscard]] std::vector<std::string_view> ids() const;
    [[nodiscard]] std::vector<std::unique_ptr<IModule>> createAll() const;

private:
    ModuleRegistry() = default;

    std::vector<std::pair<std::string, Factory>> mFactories;
};

} // namespace origin_mod::modules

// Registration macro (one-line, translation-unit local).
// Usage: ORIGINMOD_REGISTER_MODULE(origin_mod::modules::YourModule);
#define ORIGINMOD_CONCAT_INNER(a, b) a##b
#define ORIGINMOD_CONCAT(a, b)       ORIGINMOD_CONCAT_INNER(a, b)

#define ORIGINMOD_REGISTER_MODULE(ModuleType)                                                                   \
    namespace {                                                                                              \
    [[maybe_unused]] const bool ORIGINMOD_CONCAT(kOriginModModuleRegistered_, __COUNTER__) =                        \
        ::origin_mod::modules::ModuleRegistry::instance().registerModule<ModuleType>(#ModuleType);              \
    } // namespace

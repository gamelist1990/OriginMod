#pragma once

#include "mod/modules/IModule.h"

namespace origin_mod::modules {

class FeatureModule final : public IModule {
public:
    [[nodiscard]] std::string_view id() const noexcept override { return "features"; }

    [[nodiscard]] bool enable(OriginMod& mod) override;
    [[nodiscard]] bool disable(OriginMod& mod) override;
};

} // namespace origin_mod::modules

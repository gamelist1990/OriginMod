#include "mod/modules/FeatureModule.h"

#include "mod/features/FeatureManager.h"
#include "mod/modules/ModuleRegistry.h"

namespace origin_mod::modules {

bool FeatureModule::enable(OriginMod& mod) {
    origin_mod::features::FeatureManager::instance().initialize(mod);
    return true;
}

bool FeatureModule::disable(OriginMod& mod) {
    origin_mod::features::FeatureManager::instance().shutdown(mod);
    return true;
}

} // namespace origin_mod::modules

ORIGINMOD_REGISTER_MODULE(origin_mod::modules::FeatureModule);

#include "register_types.h"

#include <gdextension_interface.h>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

#include "player/player_controller.h"
#include "enemies/training_dummy.h"
#include "skills/skill_data.h"
#include "player/player_controller_3d.h"
#include "enemies/boss_controller_3d.h"
#include "enemies/hazard_zone_3d.h"
#include "enemies/projectile_3d.h"
#include "enemies/minion_controller_3d.h"
#include "enemies/koschei_controller_3d.h"
#include "enemies/kladenets_sword_3d.h"
#include "enemies/enemy_unit_3d.h"

using namespace godot;

void initialize_russian_roguelite(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    GDREGISTER_CLASS(PlayerController);
    GDREGISTER_CLASS(TrainingDummy);
    GDREGISTER_CLASS(SkillData);
    GDREGISTER_CLASS(PlayerController3D);
    GDREGISTER_CLASS(BossController3D);
    GDREGISTER_CLASS(HazardZone3D);
    GDREGISTER_CLASS(Projectile3D);
    GDREGISTER_CLASS(MinionController3D);
    GDREGISTER_CLASS(KoscheiController3D);
    GDREGISTER_CLASS(KladenetsSword3D);
    GDREGISTER_CLASS(EnemyUnit3D);
}

void uninitialize_russian_roguelite(ModuleInitializationLevel p_level) {
    if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
}

extern "C" {
GDExtensionBool GDE_EXPORT russian_roguelite_library_init(
        GDExtensionInterfaceGetProcAddress p_get_proc_address,
        GDExtensionClassLibraryPtr p_library,
        GDExtensionInitialization *r_initialization) {
    GDExtensionBinding::InitObject init_obj(p_get_proc_address, p_library, r_initialization);
    init_obj.register_initializer(initialize_russian_roguelite);
    init_obj.register_terminator(uninitialize_russian_roguelite);
    init_obj.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init_obj.init();
}
}

#include "skill_data.h"
#include <algorithm>
#include <godot_cpp/core/class_db.hpp>
using namespace godot;

void SkillData::_bind_methods() {
    ClassDB::bind_method(D_METHOD("set_skill_id", "value"), &SkillData::set_skill_id);
    ClassDB::bind_method(D_METHOD("get_skill_id"), &SkillData::get_skill_id);
    ClassDB::bind_method(D_METHOD("set_skill_name", "value"), &SkillData::set_skill_name);
    ClassDB::bind_method(D_METHOD("get_skill_name"), &SkillData::get_skill_name);
    ClassDB::bind_method(D_METHOD("set_description", "value"), &SkillData::set_description);
    ClassDB::bind_method(D_METHOD("get_description"), &SkillData::get_description);
    ClassDB::bind_method(D_METHOD("set_damage", "value"), &SkillData::set_damage);
    ClassDB::bind_method(D_METHOD("get_damage"), &SkillData::get_damage);
    ClassDB::bind_method(D_METHOD("set_cooldown", "value"), &SkillData::set_cooldown);
    ClassDB::bind_method(D_METHOD("get_cooldown"), &SkillData::get_cooldown);
    ClassDB::bind_method(D_METHOD("set_rarity", "value"), &SkillData::set_rarity);
    ClassDB::bind_method(D_METHOD("get_rarity"), &SkillData::get_rarity);
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "skill_id"), "set_skill_id", "get_skill_id");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "skill_name"), "set_skill_name", "get_skill_name");
    ADD_PROPERTY(PropertyInfo(Variant::STRING, "description"), "set_description", "get_description");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damage"), "set_damage", "get_damage");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cooldown"), "set_cooldown", "get_cooldown");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "rarity"), "set_rarity", "get_rarity");
}

void SkillData::set_skill_id(const String &value) { skill_id = value; }
String SkillData::get_skill_id() const { return skill_id; }
void SkillData::set_skill_name(const String &value) { skill_name = value; }
String SkillData::get_skill_name() const { return skill_name; }
void SkillData::set_description(const String &value) { description = value; }
String SkillData::get_description() const { return description; }
void SkillData::set_damage(double value) { damage = std::max(0.0, value); }
double SkillData::get_damage() const { return damage; }
void SkillData::set_cooldown(double value) { cooldown = std::max(0.0, value); }
double SkillData::get_cooldown() const { return cooldown; }
void SkillData::set_rarity(int value) { rarity = std::clamp(value, 1, 5); }
int SkillData::get_rarity() const { return rarity; }

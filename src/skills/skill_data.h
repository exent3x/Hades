#pragma once

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

class SkillData : public Resource {
    GDCLASS(SkillData, Resource)

private:
    String skill_id = "";
    String skill_name = "Новый навык";
    String description = "";
    double damage = 0.0;
    double cooldown = 0.0;
    int rarity = 1;

protected:
    static void _bind_methods();

public:
    void set_skill_id(const String &value);
    String get_skill_id() const;
    void set_skill_name(const String &value);
    String get_skill_name() const;
    void set_description(const String &value);
    String get_description() const;
    void set_damage(double value);
    double get_damage() const;
    void set_cooldown(double value);
    double get_cooldown() const;
    void set_rarity(int value);
    int get_rarity() const;
};

} // namespace godot

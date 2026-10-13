#pragma once

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

class Sprite3D;

// Мелкий приспешник Лешего: бежит к герою и бьёт при касании.
class MinionController3D : public CharacterBody3D {
    GDCLASS(MinionController3D, CharacterBody3D)

private:
    double max_health = 40.0;
    double health = 40.0;
    double move_speed = 2.3;
    double attack_range = 0.95;
    double contact_damage = 8.0;
    double attack_cooldown = 0.9;
    double attack_cooldown_left = 0.5;
    double hit_flash_left = 0.0;
    double knockback_decay = 14.0;

    Vector3 knockback_velocity;
    Sprite3D *sprite = nullptr;

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _physics_process(double delta) override;

    void configure(const Dictionary &p_params);
    void take_damage(double amount);
    void apply_knockback(const Vector3 &force);
    double get_health() const;
    double get_hit_radius() const;
};

} // namespace godot

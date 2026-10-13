#pragma once

#include <godot_cpp/classes/character_body3d.hpp>

namespace godot {

class Area3D;
class CollisionShape3D;
class MeshInstance3D;
class Sprite3D;

// 3D-версия PlayerController для 2.5D-режима.
// Мир: плоскость XZ (Y — вверх). Единицы — метры.
class PlayerController3D : public CharacterBody3D {
    GDCLASS(PlayerController3D, CharacterBody3D)

private:
    double base_max_health = 100.0;
    double max_health = 100.0;
    double health = 100.0;

    double move_speed = 4.6;
    double dash_speed = 15.0;
    double dash_duration = 0.14;
    double dash_cooldown = 0.60;

    double attack_duration = 0.14;
    double attack_cooldown = 0.24;
    double combo_reset_time = 0.72;
    double attack_damage_multiplier = 1.0;

    // Умение (Q): метание камня.
    double skill_damage = 40.0;
    double skill_duration = 0.20;
    double skill_cooldown = 1.8;

    // Дебаффы от босса.
    double stun_time_left = 0.0;
    double stun_immunity_left = 0.0;
    double slow_time_left = 0.0;
    double slow_factor = 1.0;
    double weaken_time_left = 0.0;
    double weaken_mult = 1.0;
    double vuln_time_left = 0.0;     // уязвимость: герой получает больше урона
    double vuln_mult = 1.0;
    double silence_time_left = 0.0;  // немота: нельзя использовать умение (Q)

    // Отбрасывание (grab_throw от Лешего, отбросы от других боссов).
    Vector3 knockback_velocity = Vector3();
    double knockback_decay = 12.0;   // м/с² — затухание импульса

    double dash_time_left = 0.0;
    double dash_cooldown_left = 0.0;
    double attack_time_left = 0.0;
    double attack_cooldown_left = 0.0;
    double attack_elapsed = 0.0;
    double combo_time_left = 0.0;
    double skill_time_left = 0.0;
    double skill_cooldown_left = 0.0;
    double hit_invulnerability_left = 0.0;

    int combo_step = 0;
    bool attack_has_hit = false;
    bool dead = false;

    Vector3 facing = Vector3(1.0, 0.0, 0.0);
    Vector3 dash_direction = Vector3(1.0, 0.0, 0.0);

    Area3D *attack_area = nullptr;
    CollisionShape3D *attack_shape = nullptr;
    MeshInstance3D *attack_visual = nullptr;

    Sprite3D *sprite = nullptr;
    Node *visual = nullptr;   // узел "Visual" (GDScript с 3D-моделью): set_tint(Color)

    void update_aim();
    void update_sprite();
    void start_dash(const Vector3 &direction);
    void start_attack();
    void finish_attack();
    void perform_attack_hit();
    void start_skill();
    double get_current_attack_damage() const;
    double get_current_attack_knockback() const;

protected:
    static void _bind_methods();

public:
    PlayerController3D() = default;
    ~PlayerController3D() = default;

    void _ready() override;
    void _physics_process(double delta) override;

    void take_damage(double amount);
    void heal(double amount);

    void increase_attack(double percent);
    void increase_max_health(double amount);
    void reduce_dash_cooldown(double amount);
    void reset_run_stats();

    // Дебаффы.
    void apply_stun(double duration);
    void apply_slow(double factor, double duration);
    void apply_weaken(double multiplier, double duration);
    void apply_vulnerable(double multiplier, double duration);
    void apply_silence(double duration);
    double get_vulnerable_left() const;
    double get_silence_left() const;
    void apply_knockback(const Vector3 &force);
    void push_out(const Vector3 &offset);   // одностороннее выталкивание (из тела босса/врагов)
    void set_health(double value);
    double get_stun_left() const;
    double get_slow_left() const;
    double get_weaken_left() const;

    double get_health() const;
    double get_max_health() const;
    double get_attack_damage_multiplier() const;
    double get_dash_cooldown() const;
    bool is_dashing() const;
    bool is_attacking() const;
    bool is_using_skill() const;
    bool is_dead() const;
    int get_combo_step() const;
    Vector3 get_facing() const;
    double get_dash_cooldown_left() const;
    double get_skill_cooldown_left() const;
};

} // namespace godot
#pragma once

#include <godot_cpp/classes/character_body2d.hpp>

namespace godot {

class Area2D;
class CollisionShape2D;
class Polygon2D;

class PlayerController : public CharacterBody2D {
    GDCLASS(PlayerController, CharacterBody2D)

private:
    double base_max_health = 100.0;
    double max_health = 100.0;
    double health = 100.0;

    double move_speed = 280.0;
    double dash_speed = 920.0;
    double dash_duration = 0.14;
    double dash_cooldown = 0.60;

    double attack_duration = 0.14;
    double attack_cooldown = 0.24;
    double combo_reset_time = 0.72;
    double attack_damage_multiplier = 1.0;

    double skill_damage = 48.0;
    double skill_knockback = 340.0;
    double skill_duration = 0.18;
    double skill_cooldown = 2.8;

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
    bool skill_has_hit = false;

    Vector2 facing = Vector2(1.0, 0.0);
    Vector2 dash_direction = Vector2(1.0, 0.0);

    Area2D *attack_area = nullptr;
    CollisionShape2D *attack_shape = nullptr;
    Polygon2D *attack_visual = nullptr;

    Area2D *skill_area = nullptr;
    CollisionShape2D *skill_shape = nullptr;
    Polygon2D *skill_visual = nullptr;

    void update_aim();
    void start_dash(const Vector2 &direction);
    void start_attack();
    void finish_attack();
    void perform_attack_hit();
    void start_skill();
    void finish_skill();
    void perform_skill_hit();
    double get_current_attack_damage() const;
    double get_current_attack_knockback() const;

protected:
    static void _bind_methods();

public:
    PlayerController() = default;
    ~PlayerController() = default;

    void _ready() override;
    void _physics_process(double delta) override;

    void take_damage(double amount);
    void heal(double amount);

    void increase_attack(double percent);
    void increase_max_health(double amount);
    void reduce_dash_cooldown(double amount);
    void reset_run_stats();

    double get_health() const;
    double get_max_health() const;
    double get_attack_damage_multiplier() const;
    double get_dash_cooldown() const;
    bool is_dashing() const;
    bool is_attacking() const;
    bool is_using_skill() const;
    bool is_dead() const;
    int get_combo_step() const;
    Vector2 get_facing() const;
    double get_dash_cooldown_left() const;
    double get_skill_cooldown_left() const;
};

} // namespace godot

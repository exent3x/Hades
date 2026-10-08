#pragma once

#include <godot_cpp/classes/character_body2d.hpp>

namespace godot {

class Polygon2D;
class ProgressBar;

class TrainingDummy : public CharacterBody2D {
    GDCLASS(TrainingDummy, CharacterBody2D)

private:
    double max_health = 80.0;
    double health = 80.0;
    double move_speed = 92.0;
    double attack_range = 52.0;
    double contact_damage = 10.0;
    double attack_cooldown = 0.82;
    double attack_cooldown_left = 0.0;
    double knockback_decay = 980.0;
    double hit_flash_left = 0.0;

    bool elite = false;
    bool boss = false;
    double special_cooldown_left = 1.4;
    double special_windup_left = 0.0;
    double special_radius = 180.0;
    double special_damage = 24.0;

    Vector2 knockback_velocity = Vector2();

    Polygon2D *body_visual = nullptr;
    Polygon2D *telegraph_visual = nullptr;
    ProgressBar *health_bar = nullptr;

    void update_boss_phase();
    void update_boss_special(double delta);

protected:
    static void _bind_methods();

public:
    TrainingDummy() = default;
    ~TrainingDummy() = default;

    void _ready() override;
    void _physics_process(double delta) override;
    void take_damage(double amount);
    void apply_knockback(const Vector2 &force);
    void set_elite(bool value);
    bool is_elite() const;
    void set_boss(bool value);
    bool is_boss() const;
    double get_health() const;
    double get_max_health() const;
};

} // namespace godot

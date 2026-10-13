#pragma once

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class Sprite3D;

// Леший (2.5D). Расширенный ИИ: intent-based движение, threat-tracking, punish,
// combo-связки, предиктивная наводка, анти-кайт, root-wall, ярость в 3-й фазе,
// стан как окно расправы, анти-застой в мили (grab_throw к стене).
class BossController3D : public CharacterBody3D {
    GDCLASS(BossController3D, CharacterBody3D)

public:
    enum Action {
        ACTION_NONE,
        ACTION_MELEE,
        ACTION_THROW_SPRUCE,
        ACTION_ROOTS,
        ACTION_BIRDS,
        ACTION_SUMMON,
        ACTION_MUSHROOMS,
        ACTION_SHRINK,
        ACTION_GRAB_THROW,
        ACTION_COUNT
    };

    enum SpruceState {
        SPRUCE_HELD,
        SPRUCE_FLYING,
        SPRUCE_GROUND
    };

    enum MoveIntent {
        MOVE_HOLD,
        MOVE_APPROACH,
        MOVE_RETREAT,
        MOVE_STRAFE,
        MOVE_TO_POINT
    };

private:
    double max_health = 900.0;
    double health = 900.0;
    double move_speed = 1.3;
    int phase = 1;
    double hit_flash_left = 0.0;
    double damage_scale = 1.0;

    // Состояние действий.
    Action action = ACTION_NONE;
    double windup_left = 0.0;
    double windup_total = 0.0;
    double recover_left = 0.0;
    double decision_left = 1.6;
    Action last_action = ACTION_NONE;

    // Перезарядки способностей.
    double cd_roots = 1.5;
    double cd_birds = 3.5;
    double cd_summon = 5.0;
    double cd_mushrooms = 1.5;
    double cd_throw = 4.0;
    double cd_shrink = 15.0;

    // Ель.
    SpruceState spruce_state = SPRUCE_HELD;
    Vector3 spruce_pos;
    double spruce_fly_left = 0.0;

    // Зафиксированные в замахе данные.
    Vector3 action_dir = Vector3(1.0, 0.0, 0.0);
    Vector3 action_target;
    bool mushroom_throw_mode = false;

    Vector2 arena_half = Vector2(8.5, 5.0);

    // Движение (intent-based).
    MoveIntent move_intent = MOVE_HOLD;
    Vector3 move_target;
    double strafe_dir = 1.0;
    double strafe_change_left = 0.0;
    double corner_escape_cooldown = 0.0;
    double cached_player_distance = 0.0;

    // Память о поведении игрока.
    double last_player_attack_time = -100.0;
    int recent_player_hits = 0;
    double last_punish_time = -100.0;
    double cd_punish = 0.0;

    // Комбо-связки.
    double combo_window_left = 0.0;
    int combo_depth = 0;

    // Предиктив движения игрока.
    Vector3 last_player_pos;
    double last_player_sample_time = 0.0;
    Vector3 player_velocity_estimate;

    // Анти-кайт и ритм боя.
    double time_since_player_hit = 0.0;
    double fight_time = 0.0;

    // Стан игрока как окно расправы.
    bool stun_attack_fired = false;

    // Анти-застой: время, проведённое игроком в мили без уклонений.
    double melee_pressure_time = 0.0;
    double cd_throw_player = 0.0;

    // Ярость (фаза 3).
    int rage_step = 0;

    Sprite3D *sprite = nullptr;
    Node *visual = nullptr;   // узел "Visual" (GDScript с 3D-моделью): set_tint(Color)
    Vector3 facing_dir = Vector3(0.0, 0.0, 1.0);
    MeshInstance3D *telegraph_visual = nullptr;
    MeshInstance3D *melee_telegraph = nullptr;

    // Логика.
    void update_phase();
    void tick_timers(double delta);
    void choose_action(Node3D *player);
    void begin_action(Action a, Node3D *player);
    void execute_action(Node3D *player);
    void update_sprite(const Vector3 &to_player);
    void hide_telegraphs();
    void push_player_out(Node3D *player, const Vector3 &to_player, double delta);

    void update_movement(double delta, Node3D *player);
    bool is_cornered() const;
    Vector3 escape_corner_point() const;
    bool player_is_dangerous(Node3D *player) const;
    void register_player_hit(double distance);
    bool should_punish() const;
    void trigger_punish();
    bool should_attack_now(Node3D *player, double dist) const;
    void spawn_root_wall(const Vector3 &center, const Vector3 &away_dir);
    double player_stun_left(Node3D *player) const;

    // Вспомогательное.
    Node3D *get_player() const;
    int count_group(const char *name) const;
    Vector3 random_arena_point(double margin) const;
    Vector3 point_near(const Vector3 &center, double min_r, double max_r, double margin) const;
    Vector3 random_far_point(const Vector3 &from, double min_distance, double margin) const;
    void request_spawn(const String &kind, const Vector3 &position, const Dictionary &data);
    double cd_scale() const;

protected:
    static void _bind_methods();

public:
    BossController3D() = default;
    ~BossController3D() = default;

    void _ready() override;
    void _physics_process(double delta) override;

    void take_damage(double amount);
    void apply_knockback(const Vector3 &force);
    void on_projectile_landed(const String &kind, const Vector3 &position);
    void set_arena_half(const Vector2 &half);

    double get_health() const;
    double get_max_health() const;
    double get_hit_radius() const;
    int get_phase() const;
    double get_windup_progress() const;
    Vector3 get_facing_dir() const;
    bool is_spruce_held() const;
    String get_spruce_state_name() const;
    String get_action_name() const;
};

} // namespace godot
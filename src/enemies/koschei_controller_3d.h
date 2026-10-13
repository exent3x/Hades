#pragma once

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <vector>

namespace godot {

class MeshInstance3D;

// Кощей Бессмертный (2.5D). Сильнее Лешего: больше HP, быстрее, больше способностей.
//
// ИИ — не "случайный выбор по весам", а утилитарный:
//  * следит за героем (кайтит / прижимается / стоит / часто дэшит / быстро ли его бьют),
//  * оценивает каждую способность по обстановке и штрафует повторы (история последних действий),
//  * строит связки (проклял -> добил когтями, прыгнул -> комбо, оглушён -> наказание),
//  * целится с упреждением (по скорости героя), даёт щадящее окно после попадания,
//  * реагирует на burst-урон и порогами HP включает временную неуязвимость.
// Сам ничего не создаёт в мире — просит сигналом spawn_requested (как Леший), меч-кладенец живёт отдельно.
class KoscheiController3D : public CharacterBody3D {
    GDCLASS(KoscheiController3D, CharacterBody3D)

public:
    enum Action {
        ACTION_NONE,
        ACTION_SWIPE,
        ACTION_COMBO,
        ACTION_LUNGE,
        ACTION_BOLTS,
        ACTION_CURSE,
        ACTION_STORM,
        ACTION_NOVA,
        ACTION_SUMMON,
        ACTION_SHIELD,
        ACTION_JUMP,
        ACTION_JUMP_AIR,
        ACTION_JUMP_LAND,
        ACTION_COUNT
    };

private:
    struct PendingHit {
        double delay = 0.0;
        double damage = 0.0;
        double reach = 3.8;
        double cos_half = 0.4;
        double step = 0.0;
        bool aimed = false;
        Vector3 dir;
    };

    double max_health = 1500.0;
    double health = 1500.0;
    int phase = 1;
    double hit_flash_left = 0.0;
    double time_now = 0.0;

    // машина действий
    Action action = ACTION_NONE;
    double windup_left = 0.0;
    double windup_total = 0.0;
    double recover_left = 0.0;
    double decision_left = 1.4;
    double cd[ACTION_COUNT];
    Action history[4];
    int storm_variant = 0;

    // удары когтями / рывки
    std::vector<PendingHit> hits;
    double dash_left = 0.0;
    double dash_speed = 0.0;
    Vector3 dash_dir;
    bool lunge_active = false;
    bool lunge_hit_done = false;
    Vector3 lunge_target;
    double lunge_length = 0.0;

    // прыжок
    Vector3 jump_from;
    Vector3 jump_to;
    bool jump_slam = false;
    bool airborne = false;

    // неуязвимость
    double shield_left = 0.0;
    bool shield_66_used = false;
    bool shield_33_used = false;
    bool force_shield = false;

    // телеметрия поведения героя
    Vector3 last_player_pos;
    bool telemetry_ready = false;
    Vector3 player_vel;
    double speed_avg = 0.0;
    double avg_dist = 5.0;
    double melee_time = 0.0;
    double idle_time = 0.0;
    double last_player_hp = -1.0;
    double since_hurt = 99.0;
    bool prev_player_dashing = false;
    std::vector<double> dash_times;
    std::vector<std::pair<double, double>> damage_log;   // (время, урон по боссу)
    double prefer_ranged_until = 0.0;
    double punish_until = 0.0;       // до этого момента ближний бой в приоритете (связка)

    double strafe_dir = 1.0;
    double strafe_left = 2.0;

    Vector3 facing_dir = Vector3(0.0, 0.0, 1.0);
    Vector2 arena_half = Vector2(8.5, 5.0);

    Node *visual = nullptr;
    MeshInstance3D *telegraph_visual = nullptr;
    MeshInstance3D *melee_telegraph = nullptr;
    MeshInstance3D *lunge_telegraph = nullptr;

    // ---- логика
    void update_phase();
    void update_telemetry(Node3D *player, double delta);
    void update_pending_hits(Node3D *player, double delta);
    void update_shield_triggers();
    double recent_damage(double window) const;
    double dash_rate() const;
    double tempo() const;
    double damage_scale() const;
    double cd_scale() const;
    void choose_action(Node3D *player);
    void begin_action(Action a, Node3D *player);
    void execute_action(Node3D *player);
    void queue_hit(double delay, double damage, double reach, double cos_half, double step);
    void push_history(Action a);
    bool in_history(Action a, int depth) const;

    // ---- помощники
    Node3D *get_player() const;
    int count_group(const char *name) const;
    void request_spawn(const String &kind, const Vector3 &position, const Dictionary &data);
    Vector3 clamp_to_arena(const Vector3 &p, double margin) const;
    Vector3 point_near(const Vector3 &center, double min_r, double max_r, double margin) const;
    Vector3 random_far_point(const Vector3 &from, double min_distance, double margin) const;
    Vector3 predicted_player(Node3D *player, double lead) const;
    void fire_bolt(const Vector3 &dir, double speed, double damage, double radius, double homing, const Dictionary &debuff);
    Dictionary choose_curse(Node3D *player);
    void spawn_blast(const Vector3 &pos, double radius, double telegraph, double damage);
    void hide_telegraphs();
    void update_visuals(const Vector3 &to_player);
    void push_player_out(Node3D *player, const Vector3 &to_player, double delta);
    void command_sword(const String &name);

protected:
    static void _bind_methods();

public:
    KoscheiController3D();
    ~KoscheiController3D() = default;

    void _ready() override;
    void _physics_process(double delta) override;

    void take_damage(double amount);
    void apply_knockback(const Vector3 &force);
    void set_arena_half(const Vector2 &half);

    double get_health() const;
    double get_max_health() const;
    double get_hit_radius() const;
    int get_phase() const;
    bool is_invulnerable() const;
    bool is_airborne() const;
    double get_shield_left() const;
    double get_windup_progress() const;
    Vector3 get_facing_dir() const;
    String get_action_name() const;
};

} // namespace godot

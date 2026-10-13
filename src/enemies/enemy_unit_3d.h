#pragma once

#include <godot_cpp/classes/character_body3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector2.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class MeshInstance3D;
class Sprite3D;
class Node3D;

// Обычные враги 3D-комнат. Одна сцена-класс, четыре роли и элитная версия каждой.
//
//  brute  — медленный, много HP, тяжёлый телеграфируемый удар (элита: землетряс-удар по области, броня при замахе, ярость).
//  raider — быстрый, мало HP: кружит, влетает двойным ударом и отходит; уклоняется от замаха героя
//           (элита: "теневой шаг" — блинк за спину героя).
//  jumper — самоубийца: приседает, метка приземления фиксируется перед прыжком, взрывается при приземлении
//           (от прыжка можно увернуться; элита: большой взрыв, горящая земля, взрывается и при гибели).
//  archer — дальник: держит дистанцию, отступает, стреляет с упреждением и видимым прицелом
//           (элита: залп из трёх замедляющих болтов и блинк-отскок, когда его прижали).
//
// Тактика группы: ограничение числа одновременно атакующих в ближнем бою (остальные кружат и ждут),
// расталкивание друг от друга, обход застревания у препятствий.
class EnemyUnit3D : public CharacterBody3D {
    GDCLASS(EnemyUnit3D, CharacterBody3D)

public:
    enum Role { ROLE_BRUTE, ROLE_RAIDER, ROLE_JUMPER, ROLE_ARCHER };
    enum State {
        ST_APPROACH,
        ST_WINDUP,
        ST_STRIKE_CHAIN,
        ST_RECOVER,
        ST_RETREAT,
        ST_PRIME,
        ST_LEAP,
        ST_BLINK,
        ST_DODGE
    };

private:
    Role role = ROLE_BRUTE;
    bool elite = false;
    double max_health = 100.0;
    double health = 100.0;
    double base_speed = 2.0;
    double dmg = 10.0;
    double hit_flash_left = 0.0;
    double time_now = 0.0;

    State state = ST_APPROACH;
    double state_time = 0.0;
    double attack_cd = 1.0;
    double special_cd = 3.0;
    double dodge_cd = 0.0;
    int attack_count = 0;
    bool enraged = false;
    bool slam_attack = false;

    double strafe_dir = 1.0;
    double strafe_left = 1.5;
    Vector3 aim_dir = Vector3(0.0, 0.0, 1.0);
    Vector3 target_pos;
    Vector3 leap_from;
    int chain_hits_left = 0;
    double chain_timer = 0.0;
    Vector3 knockback_velocity;
    Vector3 dodge_dir;

    // защита от застревания
    Vector3 stuck_ref;
    double stuck_timer = 0.0;
    double unstick_left = 0.0;
    double unstick_sign = 1.0;

    Vector2 arena_half = Vector2(7.5, 5.0);

    Sprite3D *sprite = nullptr;
    Node3D *hp_fill = nullptr;
    Node3D *hp_root = nullptr;
    MeshInstance3D *tele_cone = nullptr;
    MeshInstance3D *tele_disc = nullptr;
    MeshInstance3D *tele_line = nullptr;

    // ---- общее
    Node3D *get_player() const;
    Vector3 separation() const;
    int count_attackers() const;
    bool can_start_melee() const;
    Vector3 clamp_to_arena(const Vector3 &p, double margin) const;
    Vector3 predicted(Node3D *player, double lead) const;
    void request_spawn(const String &kind, const Vector3 &position, const Dictionary &data);
    void set_state(State s);
    void hide_telegraphs();
    void update_visual(const Vector3 &to_player);
    void melee_hit(Node3D *player, double reach, double cos_half, double amount);
    void spawn_blast(const Vector3 &pos, double radius, double telegraph, double damage);
    void die();
    void move(const Vector3 &dir, double speed, double delta);
    void stop_move();

    // ---- роли
    void think_brute(Node3D *player, const Vector3 &to_player, double dist, double delta);
    void think_raider(Node3D *player, const Vector3 &to_player, double dist, double delta);
    void think_jumper(Node3D *player, const Vector3 &to_player, double dist, double delta);
    void think_archer(Node3D *player, const Vector3 &to_player, double dist, double delta);
    void fire_arrows(Node3D *player);
    Vector3 circle_dir(const Vector3 &to_player, double dist, double ideal, double delta);

protected:
    static void _bind_methods();

public:
    EnemyUnit3D() = default;
    ~EnemyUnit3D() = default;

    void _ready() override;
    void _physics_process(double delta) override;

    void configure(const Dictionary &p_params);
    void take_damage(double amount);
    void apply_knockback(const Vector3 &force);
    void set_arena_half(const Vector2 &half);

    double get_health() const;
    double get_max_health() const;
    double get_hit_radius() const;
    bool is_attacking() const;
    bool is_elite() const;
    String get_role_name() const;
};

} // namespace godot

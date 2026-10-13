#include <algorithm>
#include <cmath>

#include "enemy_unit_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ----- баланс: HP / скорость / урон (герой: 100 HP, 4.6 м/с) -----
static const double BRUTE_HP = 150.0, BRUTE_SPEED = 1.7, BRUTE_DMG = 28.0, BRUTE_REACH = 2.8;
static const double RAIDER_HP = 45.0, RAIDER_SPEED = 4.4, RAIDER_DMG = 9.0, RAIDER_REACH = 2.0;
static const double JUMPER_HP = 30.0, JUMPER_SPEED = 3.4, JUMPER_DMG = 22.0, JUMPER_RADIUS = 2.0;
static const double ARCHER_HP = 40.0, ARCHER_SPEED = 2.6, ARCHER_DMG = 11.0;
static const double ELITE_HP_MULT = 2.4, ELITE_DMG_MULT = 1.35;
static const double MELEE_TOKENS = 2.0;      // сколько врагов одновременно атакуют вблизи

void EnemyUnit3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "params"), &EnemyUnit3D::configure);
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &EnemyUnit3D::take_damage);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &EnemyUnit3D::apply_knockback);
    ClassDB::bind_method(D_METHOD("set_arena_half", "half"), &EnemyUnit3D::set_arena_half);
    ClassDB::bind_method(D_METHOD("get_health"), &EnemyUnit3D::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &EnemyUnit3D::get_max_health);
    ClassDB::bind_method(D_METHOD("get_hit_radius"), &EnemyUnit3D::get_hit_radius);
    ClassDB::bind_method(D_METHOD("is_attacking"), &EnemyUnit3D::is_attacking);
    ClassDB::bind_method(D_METHOD("is_elite"), &EnemyUnit3D::is_elite);
    ClassDB::bind_method(D_METHOD("get_role_name"), &EnemyUnit3D::get_role_name);
    ADD_SIGNAL(MethodInfo("spawn_requested",
            PropertyInfo(Variant::STRING, "kind"),
            PropertyInfo(Variant::VECTOR3, "position"),
            PropertyInfo(Variant::DICTIONARY, "data")));
    ADD_SIGNAL(MethodInfo("died_unit"));
}

void EnemyUnit3D::configure(const Dictionary &p_params) {
    const String r = String(p_params.get("role", "brute"));
    if (r == String("raider")) role = ROLE_RAIDER;
    else if (r == String("jumper")) role = ROLE_JUMPER;
    else if (r == String("archer")) role = ROLE_ARCHER;
    else role = ROLE_BRUTE;
    elite = bool(p_params.get("elite", false));
    const double hp_mult = double(p_params.get("hp_mult", 1.0));
    const double dmg_mult = double(p_params.get("dmg_mult", 1.0));
    const double speed_mult = double(p_params.get("speed_mult", 1.0));

    switch (role) {
        case ROLE_BRUTE: max_health = BRUTE_HP; base_speed = BRUTE_SPEED; dmg = BRUTE_DMG; break;
        case ROLE_RAIDER: max_health = RAIDER_HP; base_speed = RAIDER_SPEED; dmg = RAIDER_DMG; break;
        case ROLE_JUMPER: max_health = JUMPER_HP; base_speed = JUMPER_SPEED; dmg = JUMPER_DMG; break;
        default: max_health = ARCHER_HP; base_speed = ARCHER_SPEED; dmg = ARCHER_DMG; break;
    }
    if (elite) {
        max_health *= ELITE_HP_MULT;
        dmg *= ELITE_DMG_MULT;
        base_speed *= 1.08;
    }
    max_health *= hp_mult;
    dmg *= dmg_mult;
    base_speed *= speed_mult;
    health = max_health;
    attack_cd = UtilityFunctions::randf_range(0.6, 1.8);   // чтобы волна не ударила синхронно
    special_cd = UtilityFunctions::randf_range(2.0, 4.0);
    strafe_dir = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
}

void EnemyUnit3D::_ready() {
    add_to_group(StringName("enemies"));
    add_to_group(StringName("room_enemies"));
    sprite = Object::cast_to<Sprite3D>(get_node_or_null(NodePath("Sprite")));
    hp_root = Object::cast_to<Node3D>(get_node_or_null(NodePath("HPBar")));
    hp_fill = Object::cast_to<Node3D>(get_node_or_null(NodePath("HPBar/Fill")));
    tele_cone = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("TeleCone")));
    tele_disc = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("TeleDisc")));
    tele_line = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("TeleLine")));
    set_motion_mode(CharacterBody3D::MOTION_MODE_FLOATING);
    stuck_ref = get_global_position();
    hide_telegraphs();
}

// ===================================================================== общие помощники

Node3D *EnemyUnit3D::get_player() const { return find_player_3d(this); }

Vector3 EnemyUnit3D::clamp_to_arena(const Vector3 &p, double margin) const {
    Vector3 r = p;
    r.x = real_t(std::clamp(double(r.x), -(double(arena_half.x) - margin), double(arena_half.x) - margin));
    r.z = real_t(std::clamp(double(r.z), -(double(arena_half.y) - margin), double(arena_half.y) - margin));
    return r;
}

Vector3 EnemyUnit3D::predicted(Node3D *player, double lead) const {
    Vector3 v;
    if (player->has_method(StringName("get_facing")) && player->has_method(StringName("is_dashing"))) {
        // грубое упреждение: берём скорость героя из CharacterBody3D.velocity, если доступна
        Variant vel = player->get(StringName("velocity"));
        if (vel.get_type() == Variant::VECTOR3) v = Vector3(vel);
    }
    return clamp_to_arena(player->get_global_position() + Vector3(v.x, 0.0, v.z) * lead, 0.6);
}

void EnemyUnit3D::request_spawn(const String &kind, const Vector3 &position, const Dictionary &data) {
    emit_signal(StringName("spawn_requested"), kind, position, data);
}

void EnemyUnit3D::spawn_blast(const Vector3 &pos, double radius, double telegraph, double damage) {
    Dictionary d;
    d["kind"] = String("blast");
    d["radius"] = radius;
    d["telegraph"] = telegraph;
    d["active"] = 0.45;
    d["damage"] = damage;
    d["explodes"] = true;
    request_spawn("blast", pos, d);
}

void EnemyUnit3D::set_state(State s) {
    state = s;
    state_time = 0.0;
}

void EnemyUnit3D::hide_telegraphs() {
    if (tele_cone != nullptr) tele_cone->set_visible(false);
    if (tele_line != nullptr) tele_line->set_visible(false);
    if (tele_disc != nullptr) {
        tele_disc->set_visible(false);
        tele_disc->set_position(Vector3(0.0, 0.05, 0.0));
    }
}

// Расталкивание: враги не слипаются в одну точку.
Vector3 EnemyUnit3D::separation() const {
    Vector3 push;
    if (!is_inside_tree()) return push;
    const Array all = get_tree()->get_nodes_in_group(StringName("room_enemies"));
    for (int i = 0; i < all.size(); ++i) {
        Node3D *other = Object::cast_to<Node3D>(all[i]);
        if (other == nullptr || other == this) continue;
        Vector3 d = get_global_position() - other->get_global_position();
        d.y = 0.0;
        const double len = d.length();
        if (len < 1.5 && len > 0.001) push += d / len * ((1.5 - len) / 1.5);
    }
    return push;
}

// Тактика группы: вблизи одновременно бьют не больше MELEE_TOKENS врагов (при низком HP героя — один).
int EnemyUnit3D::count_attackers() const {
    int n = 0;
    if (!is_inside_tree()) return 0;
    const Array all = get_tree()->get_nodes_in_group(StringName("room_enemies"));
    for (int i = 0; i < all.size(); ++i) {
        EnemyUnit3D *other = Object::cast_to<EnemyUnit3D>(all[i]);
        if (other != nullptr && other != this && other->is_attacking() &&
                (other->role == ROLE_BRUTE || other->role == ROLE_RAIDER)) ++n;
    }
    return n;
}

bool EnemyUnit3D::can_start_melee() const {
    double limit = MELEE_TOKENS;
    Node3D *player = get_player();
    if (player != nullptr && player->has_method(StringName("get_health")) && player->has_method(StringName("get_max_health"))) {
        const double mx = double(player->call(StringName("get_max_health")));
        if (mx > 0.0 && double(player->call(StringName("get_health"))) / mx < 0.35) limit = 1.0;   // добивание по очереди — без "сливания"
    }
    return double(count_attackers()) < limit;
}

void EnemyUnit3D::move(const Vector3 &dir, double speed, double delta) {
    Vector3 d = dir;
    d.y = 0.0;
    if (unstick_left > 0.0) d = d.rotated(Vector3(0.0, 1.0, 0.0), unstick_sign * 1.3);
    if (speed > 0.5) stuck_timer += 0.0;   // (учёт застревания — в _physics_process)
    set_velocity(d * speed + separation() * 1.8 + knockback_velocity);
    move_and_slide();
    set_global_position(clamp_to_arena(get_global_position(), 0.5));
    (void)delta;
}

void EnemyUnit3D::stop_move() {
    set_velocity(knockback_velocity);
    move_and_slide();
}

void EnemyUnit3D::melee_hit(Node3D *player, double reach, double cos_half, double amount) {
    if (player == nullptr || !player->has_method(StringName("take_damage"))) return;
    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    const double dist = to_player.length();
    if (dist > reach || dist < 0.001) return;
    if (to_player.normalized().dot(aim_dir) < cos_half) return;
    player->call(StringName("take_damage"), amount);
}

// ===================================================================== основной цикл

void EnemyUnit3D::_physics_process(double delta) {
    time_now += delta;
    state_time += delta;
    hit_flash_left = std::max(0.0, hit_flash_left - delta);
    attack_cd = std::max(0.0, attack_cd - delta);
    special_cd = std::max(0.0, special_cd - delta);
    dodge_cd = std::max(0.0, dodge_cd - delta);
    unstick_left = std::max(0.0, unstick_left - delta);
    knockback_velocity = knockback_velocity.move_toward(Vector3(), 16.0 * delta);

    if (elite && role == ROLE_BRUTE && !enraged && health < max_health * 0.5) enraged = true;   // элитный громила звереет

    Node3D *player = get_player();
    Vector3 to_player;
    double dist = 99.0;
    bool player_dead = false;
    if (player != nullptr) {
        to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
        dist = to_player.length();
        player_dead = player->has_method(StringName("is_dead")) && bool(player->call(StringName("is_dead")));
    }
    update_visual(to_player);

    if (player == nullptr || player_dead) {
        stop_move();
        return;
    }

    // --- застревание у препятствий: если хотели идти, но почти не сдвинулись — на время уходим в сторону ---
    stuck_timer += delta;
    if (stuck_timer >= 0.7) {
        const bool wants_move = (state == ST_APPROACH || state == ST_RETREAT);
        if (wants_move && get_global_position().distance_to(stuck_ref) < 0.18) {
            unstick_left = 0.7;
            unstick_sign = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
        }
        stuck_ref = get_global_position();
        stuck_timer = 0.0;
    }

    switch (role) {
        case ROLE_BRUTE: think_brute(player, to_player, dist, delta); break;
        case ROLE_RAIDER: think_raider(player, to_player, dist, delta); break;
        case ROLE_JUMPER: think_jumper(player, to_player, dist, delta); break;
        default: think_archer(player, to_player, dist, delta); break;
    }
}

// Движение по кругу вокруг героя на идеальной дистанции (ждёт своей очереди на удар / обходит с фланга).
Vector3 EnemyUnit3D::circle_dir(const Vector3 &to_player, double dist, double ideal, double delta) {
    strafe_left -= delta;
    if (strafe_left <= 0.0) {
        strafe_dir = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
        strafe_left = UtilityFunctions::randf_range(1.2, 2.8);
    }
    const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);
    const Vector3 side = Vector3(-dir.z, 0.0, dir.x) * real_t(strafe_dir);
    const double radial = std::clamp((dist - ideal) * 0.6, -1.0, 1.0);
    return (side * 0.8 + dir * real_t(radial)).normalized();
}

// ===================================================================== БРУТ (медленный, живучий, тяжёлый удар)

void EnemyUnit3D::think_brute(Node3D *player, const Vector3 &to_player, double dist, double delta) {
    const double speed = base_speed * (enraged ? 1.3 : 1.0);
    const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);

    switch (state) {
        case ST_APPROACH: {
            if (dist <= BRUTE_REACH + 0.5 && attack_cd <= 0.0 && can_start_melee()) {
                aim_dir = dir;
                attack_count++;
                slam_attack = elite && (attack_count % 3 == 0);       // каждый третий удар элиты — землетряс
                set_state(ST_WINDUP);
                if (slam_attack) {
                    if (tele_disc != nullptr) {
                        tele_disc->set_position(Vector3(0.0, 0.05, 0.0));
                        tele_disc->set_scale(Vector3(3.6, 1.0, 3.6));
                        tele_disc->set_visible(true);
                    }
                } else if (tele_cone != nullptr) {
                    tele_cone->set_scale(Vector3(BRUTE_REACH, 1.0, 2.6));
                    tele_cone->set_visible(true);
                }
                stop_move();
            } else if (dist > 3.4 || (attack_cd <= 0.0 && can_start_melee())) {
                move(dir, speed, delta);                              // давит вперёд
            } else {
                move(circle_dir(to_player, dist, 3.2, delta), speed * 0.8, delta);   // ждёт очереди: кружит рядом
            }
        } break;

        case ST_WINDUP: {
            const double windup = (slam_attack ? 0.95 : 0.75) * (enraged ? 0.8 : 1.0);
            if (state_time < windup * 0.6 && dist > 0.001) aim_dir = dir;      // следит за героем, потом фиксирует
            if (tele_cone != nullptr && tele_cone->is_visible()) tele_cone->set_rotation(Vector3(0.0, std::atan2(-aim_dir.z, aim_dir.x), 0.0));
            stop_move();
            if (state_time >= windup) {
                hide_telegraphs();
                if (slam_attack) {
                    spawn_blast(get_global_position(), 3.6, 0.02, dmg);
                    if (dist <= 3.6 && player->has_method(StringName("apply_knockback"))) player->call(StringName("apply_knockback"), dir * 9.0);
                } else {
                    melee_hit(player, BRUTE_REACH, 0.45, dmg);
                }
                set_state(ST_RECOVER);
            }
        } break;

        case ST_RECOVER: {
            stop_move();
            if (state_time >= (enraged ? 0.7 : 0.95)) {          // длинное восстановление — окно для контратаки
                attack_cd = enraged ? 0.5 : 1.0;
                set_state(ST_APPROACH);
            }
        } break;

        default: set_state(ST_APPROACH); break;
    }
}

// ===================================================================== НАЛЁТЧИК (быстрый, мало HP; бей-и-отступай)

void EnemyUnit3D::think_raider(Node3D *player, const Vector3 &to_player, double dist, double delta) {
    const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);

    switch (state) {
        case ST_APPROACH: {
            // Реакция на замах героя: с шансом уходит в боковой рывок (уклоняется)
            const bool player_attacking = player->has_method(StringName("is_attacking")) && bool(player->call(StringName("is_attacking")));
            if (player_attacking && dist < 3.2 && dodge_cd <= 0.0 && UtilityFunctions::randf() < 0.55) {
                dodge_dir = Vector3(-dir.z, 0.0, dir.x) * real_t((UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0);
                dodge_cd = 2.5;
                set_state(ST_DODGE);
                return;
            }
            if (attack_cd <= 0.0 && dist < 7.0 && can_start_melee()) {
                if (elite && special_cd <= 0.0 && dist > 2.4) {
                    // Теневой шаг: метка за спиной героя -> блинк -> серия ударов
                    Vector3 facing = Vector3(0.0, 0.0, 1.0);
                    if (player->has_method(StringName("get_facing"))) facing = Vector3(player->call(StringName("get_facing")));
                    facing.y = 0.0;
                    if (facing.length_squared() < 0.01) facing = dir;
                    target_pos = clamp_to_arena(player->get_global_position() - facing.normalized() * 1.7, 0.7);
                    if (tele_disc != nullptr) {
                        tele_disc->set_global_position(target_pos + Vector3(0.0, 0.05, 0.0));
                        tele_disc->set_scale(Vector3(1.3, 1.0, 1.3));
                        tele_disc->set_visible(true);
                    }
                    special_cd = 6.0;
                    set_state(ST_BLINK);
                    return;
                }
                aim_dir = dir;
                set_state(ST_WINDUP);
                return;
            }
            // Ждёт: кружит на 3 м, заходя с фланга; издалека — бежит
            if (dist > 5.5) move(dir, base_speed, delta);
            else move(circle_dir(to_player, dist, 3.0, delta), base_speed * 0.9, delta);
        } break;

        case ST_DODGE: {
            set_velocity(dodge_dir * 9.0 + knockback_velocity);
            move_and_slide();
            set_global_position(clamp_to_arena(get_global_position(), 0.5));
            if (state_time >= 0.22) set_state(ST_APPROACH);
        } break;

        case ST_BLINK: {
            stop_move();
            if (state_time >= 0.55) {
                hide_telegraphs();
                set_global_position(Vector3(target_pos.x, 0.0, target_pos.z));
                Vector3 d = player->get_global_position() - get_global_position();
                d.y = 0.0;
                aim_dir = d.length_squared() > 0.001 ? d.normalized() : dir;
                set_state(ST_WINDUP);
            }
        } break;

        case ST_WINDUP: {
            stop_move();
            if (dist > 0.001) aim_dir = dir;
            const double windup = (elite && state_time < 0.0) ? 0.2 : 0.28;
            if (state_time >= windup) {
                chain_hits_left = elite ? 3 : 2;
                chain_timer = 0.0;
                set_state(ST_STRIKE_CHAIN);
            }
        } break;

        case ST_STRIKE_CHAIN: {
            chain_timer -= delta;
            if (chain_timer <= 0.0 && chain_hits_left > 0) {
                // рывок на героя и удар
                if (dist > 0.001) aim_dir = dir;
                if (dist > 1.4) {
                    set_velocity(aim_dir * 13.0);
                    move_and_slide();
                }
                melee_hit(player, RAIDER_REACH + 0.4, 0.15, dmg);
                --chain_hits_left;
                chain_timer = 0.22;
            } else {
                set_velocity(Vector3());
                move_and_slide();
            }
            set_global_position(clamp_to_arena(get_global_position(), 0.5));
            if (chain_hits_left <= 0 && chain_timer <= 0.0) set_state(ST_RETREAT);
        } break;

        case ST_RETREAT: {
            // Отступает по дуге, не давая ответить (бей-и-беги)
            const Vector3 away = (-dir * 0.8 + Vector3(-dir.z, 0.0, dir.x) * real_t(strafe_dir) * 0.6).normalized();
            move(away, base_speed * 1.15, delta);
            if (state_time >= 0.9) {
                attack_cd = UtilityFunctions::randf_range(1.2, 2.2);
                set_state(ST_APPROACH);
            }
        } break;

        default: set_state(ST_APPROACH); break;
    }
}

// ===================================================================== ПРЫГУН-САМОУБИЙЦА

void EnemyUnit3D::think_jumper(Node3D *player, const Vector3 &to_player, double dist, double delta) {
    const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);
    const double radius = elite ? 3.0 : JUMPER_RADIUS;

    switch (state) {
        case ST_APPROACH: {
            if (dist <= 7.0 && attack_cd <= 0.0) {
                target_pos = predicted(player, 0.55);
                set_state(ST_PRIME);
                if (tele_disc != nullptr) {
                    tele_disc->set_global_position(target_pos + Vector3(0.0, 0.05, 0.0));
                    tele_disc->set_scale(Vector3(radius, 1.0, radius));
                    tele_disc->set_visible(true);
                }
                stop_move();
            } else {
                // подбегает "зигзагом", чтобы труднее было сбить на подходе
                const Vector3 side = Vector3(-dir.z, 0.0, dir.x) * real_t(std::sin(time_now * 3.2));
                move((dir + side * 0.5).normalized(), base_speed, delta);
            }
        } break;

        case ST_PRIME: {
            // Приседает и "горит": метка следит за героем, но фиксируется за 0.28 с до прыжка — успеешь уйти.
            const double prime = elite ? 0.85 : 0.7;
            if (state_time < prime - 0.28) target_pos = predicted(player, 0.55);
            if (tele_disc != nullptr) tele_disc->set_global_position(target_pos + Vector3(0.0, 0.05, 0.0));
            stop_move();
            if (state_time >= prime) {
                leap_from = get_global_position();
                set_state(ST_LEAP);
            }
        } break;

        case ST_LEAP: {
            const double flight = 0.55;
            const double t = std::clamp(state_time / flight, 0.0, 1.0);
            Vector3 p = leap_from.lerp(target_pos, t);
            p.y = 2.2 * 4.0 * t * (1.0 - t);
            set_global_position(p);
            if (t >= 1.0) {
                // приземление: взрыв по метке (вся логика урона — в HazardZone3D "blast"), прыгун погибает
                spawn_blast(Vector3(target_pos.x, 0.0, target_pos.z), radius, 0.02, dmg);
                if (elite) {
                    Dictionary d;
                    d["kind"] = String("miasma");
                    d["radius"] = radius * 0.9;
                    d["telegraph"] = 0.3;
                    d["active"] = 4.5;
                    d["damage"] = 5.0 * ELITE_DMG_MULT;
                    d["tick"] = 0.5;
                    d["slow_factor"] = 0.7;
                    d["slow_time"] = 0.8;
                    request_spawn("miasma", Vector3(target_pos.x, 0.0, target_pos.z), d);   // горящая земля
                }
                hide_telegraphs();
                die();
            }
        } break;

        default: set_state(ST_APPROACH); break;
    }
}

// ===================================================================== СТРЕЛОК (дальник)

void EnemyUnit3D::fire_arrows(Node3D *player) {
    (void)player;
    const int n = elite ? 3 : 1;
    for (int i = 0; i < n; ++i) {
        const double ang = (double(i) - double(n - 1) * 0.5) * 0.22;
        const Vector3 d = aim_dir.rotated(Vector3(0.0, 1.0, 0.0), ang);
        Dictionary data;
        const Vector3 from = get_global_position() + Vector3(0.0, 1.2, 0.0);
        data["kind"] = String("bolt");
        data["arc"] = false;
        data["from"] = from;
        data["direction"] = d;
        data["speed"] = 12.0;
        data["max_distance"] = 30.0;
        data["damage"] = dmg;
        data["radius"] = 0.3;
        data["homing"] = 0.0;
        data["style"] = String("arrow");
        if (elite) {                         // болты элиты замедляют
            data["slow_factor"] = 0.62;
            data["slow_time"] = 2.5;
        }
        request_spawn("projectile", from, data);
    }
}

void EnemyUnit3D::think_archer(Node3D *player, const Vector3 &to_player, double dist, double delta) {
    const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);

    switch (state) {
        case ST_APPROACH: {
            // Элита: если прижали — блинк-отскок на дальнюю позицию
            if (elite && special_cd <= 0.0 && dist < 3.6) {
                Vector3 best = clamp_to_arena(get_global_position() - dir * 7.0, 0.8);
                for (int i = 0; i < 8; ++i) {
                    const Vector3 c = clamp_to_arena(Vector3(UtilityFunctions::randf_range(-arena_half.x, arena_half.x), 0.0,
                                                             UtilityFunctions::randf_range(-arena_half.y, arena_half.y)), 0.8);
                    if ((c - player->get_global_position()).length() >= 7.5) { best = c; break; }
                }
                target_pos = best;
                if (tele_disc != nullptr) {
                    tele_disc->set_global_position(target_pos + Vector3(0.0, 0.05, 0.0));
                    tele_disc->set_scale(Vector3(0.9, 1.0, 0.9));
                    tele_disc->set_visible(true);
                }
                special_cd = 7.0;
                set_state(ST_BLINK);
                return;
            }
            if (attack_cd <= 0.0 && dist >= 3.0 && dist <= 13.0) {
                set_state(ST_WINDUP);
                aim_dir = (predicted(player, 0.3) - get_global_position()).normalized();
                if (tele_line != nullptr) {
                    tele_line->set_scale(Vector3(13.0, 1.0, 0.16));
                    tele_line->set_visible(true);
                }
                stop_move();
                return;
            }
            // Держит дистанцию 6.5–10: ближе — отступает по дуге, дальше — приближается, иначе стрейфит
            if (dist < 5.4) {
                const Vector3 away = (-dir * 0.9 + Vector3(-dir.z, 0.0, dir.x) * real_t(strafe_dir) * 0.5).normalized();
                move(away, base_speed * 1.2, delta);
            } else if (dist > 10.0) {
                move(dir, base_speed, delta);
            } else {
                move(circle_dir(to_player, dist, 8.0, delta), base_speed * 0.7, delta);
            }
        } break;

        case ST_BLINK: {
            stop_move();
            if (state_time >= 0.4) {
                hide_telegraphs();
                set_global_position(Vector3(target_pos.x, 0.0, target_pos.z));
                attack_cd = 0.5;
                set_state(ST_APPROACH);
            }
        } break;

        case ST_WINDUP: {
            stop_move();
            const double windup = 0.7;
            if (state_time < windup - 0.2) aim_dir = (predicted(player, 0.3) - get_global_position()).normalized();   // последние 0.2 с прицел зафиксирован
            if (tele_line != nullptr && tele_line->is_visible()) tele_line->set_rotation(Vector3(0.0, std::atan2(-aim_dir.z, aim_dir.x), 0.0));
            if (state_time >= windup) {
                hide_telegraphs();
                fire_arrows(player);
                attack_cd = elite ? 2.6 : 2.2;
                set_state(ST_RECOVER);
            }
        } break;

        case ST_RECOVER: {
            stop_move();
            if (state_time >= 0.35) set_state(ST_APPROACH);
        } break;

        default: set_state(ST_APPROACH); break;
    }
}

// ===================================================================== визуал, урон, смерть

void EnemyUnit3D::update_visual(const Vector3 &to_player) {
    if (sprite != nullptr) {
        if (std::fabs(to_player.x) > 0.25) sprite->set_flip_h(to_player.x < 0.0);
        Color tint = elite ? Color(1.0, 0.86, 0.55, 1.0) : Color(1.0, 1.0, 1.0, 1.0);
        if (state == ST_PRIME) {
            const double pulse = 0.5 + 0.5 * std::sin(time_now * 26.0);
            tint = Color(1.0, 0.35 + 0.35 * pulse, 0.25, 1.0);                 // самоубийца "горит" перед прыжком
        } else if (state == ST_WINDUP && role == ROLE_BRUTE) {
            tint = Color(1.0, 0.62, 0.55, 1.0);
        } else if (enraged) {
            tint = Color(1.0, 0.55, 0.45, 1.0);
        }
        if (hit_flash_left > 0.0) tint = Color(tint.r * 1.6, tint.g * 1.6, tint.b * 1.6, 1.0);
        sprite->set_modulate(tint);
    }
    if (hp_root != nullptr) {
        hp_root->set_visible(health < max_health - 0.5);
        if (hp_fill != nullptr) {
            const double ratio = std::clamp(health / std::max(1.0, max_health), 0.0, 1.0);
            hp_fill->set_scale(Vector3(real_t(std::max(0.001, ratio)), 1.0, 1.0));
            hp_fill->set_position(Vector3(real_t((ratio - 1.0) * 0.5), 0.0, 0.001));
        }
    }
}

void EnemyUnit3D::take_damage(double amount) {
    if (amount <= 0.0 || health <= 0.0) return;
    if (elite && role == ROLE_BRUTE && state == ST_WINDUP) amount *= 0.5;   // броня элитного громилы при замахе
    health = std::max(0.0, health - amount);
    hit_flash_left = 0.08;
    if (health <= 0.0) {
        if (elite && role == ROLE_JUMPER && state != ST_LEAP) {
            spawn_blast(get_global_position(), 2.4, 0.6, dmg * 0.8);          // подрывник взрывается и при гибели
        }
        die();
    }
}

void EnemyUnit3D::apply_knockback(const Vector3 &force) {
    if (health <= 0.0) return;
    Vector3 f = force;
    f.y = 0.0;
    // тяжёлые враги почти не отлетают; прыгун в воздухе — не сбивается
    if (state == ST_LEAP) return;
    if (role == ROLE_BRUTE) f *= elite ? 0.15 : 0.35;
    knockback_velocity += f;
}

void EnemyUnit3D::die() {
    hide_telegraphs();
    emit_signal(StringName("died_unit"));
    queue_free();
}

void EnemyUnit3D::set_arena_half(const Vector2 &half) { arena_half = half; }
double EnemyUnit3D::get_health() const { return health; }
double EnemyUnit3D::get_max_health() const { return max_health; }
double EnemyUnit3D::get_hit_radius() const {
    const double base = (role == ROLE_BRUTE) ? 0.9 : (role == ROLE_JUMPER ? 0.6 : 0.7);
    return elite ? base * 1.3 : base;
}
bool EnemyUnit3D::is_attacking() const {
    return state == ST_WINDUP || state == ST_STRIKE_CHAIN || state == ST_BLINK;
}
bool EnemyUnit3D::is_elite() const { return elite; }
String EnemyUnit3D::get_role_name() const {
    switch (role) {
        case ROLE_BRUTE: return "brute";
        case ROLE_RAIDER: return "raider";
        case ROLE_JUMPER: return "jumper";
        default: return "archer";
    }
}

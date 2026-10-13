#include <algorithm>
#include <cmath>

#include "boss_controller_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ----- Баланс (метры / секунды) -----
static const double MELEE_RANGE = 3.4;
static const double MELEE_PREFERRED = 2.4;
static const double MELEE_COS_HALF_ANGLE = 0.42;
static const double MELEE_DAMAGE = 34.0;
static const double SPRUCE_DAMAGE = 40.0;
static const double SPRUCE_RADIUS = 1.4;
static const double SPRUCE_PICKUP_RADIUS = 1.9;
static const int MAX_MINIONS = 4;

// Тело босса — твёрдое для героя: герой выталкивается из него, а босса герой не двигает.
static const double BODY_PUSH_RADIUS = 1.25;
static const double PLAYER_BODY_RADIUS = 0.3;
static const double BODY_PUSH_STIFFNESS = 16.0;

// Отброс игрока (анти-застой в мили).
static const double MELEE_PRESSURE_LIMIT  = 3.0;
static const double THROW_PLAYER_WINDUP   = 0.6;
static const double THROW_PLAYER_DAMAGE   = 15.0;
static const double THROW_PLAYER_STUN     = 0.35;
static const double THROW_PLAYER_IMPULSE  = 26.0;
static const double THROW_PLAYER_CD       = 8.0;

void BossController3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &BossController3D::take_damage);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &BossController3D::apply_knockback);
    ClassDB::bind_method(D_METHOD("on_projectile_landed", "kind", "position"), &BossController3D::on_projectile_landed);
    ClassDB::bind_method(D_METHOD("set_arena_half", "half"), &BossController3D::set_arena_half);
    ClassDB::bind_method(D_METHOD("get_health"), &BossController3D::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &BossController3D::get_max_health);
    ClassDB::bind_method(D_METHOD("get_hit_radius"), &BossController3D::get_hit_radius);
    ClassDB::bind_method(D_METHOD("get_phase"), &BossController3D::get_phase);
    ClassDB::bind_method(D_METHOD("get_windup_progress"), &BossController3D::get_windup_progress);
    ClassDB::bind_method(D_METHOD("get_facing_dir"), &BossController3D::get_facing_dir);
    ClassDB::bind_method(D_METHOD("is_spruce_held"), &BossController3D::is_spruce_held);
    ClassDB::bind_method(D_METHOD("get_spruce_state_name"), &BossController3D::get_spruce_state_name);
    ClassDB::bind_method(D_METHOD("get_action_name"), &BossController3D::get_action_name);

    ADD_SIGNAL(MethodInfo("defeated"));
    ADD_SIGNAL(MethodInfo("spruce_changed", PropertyInfo(Variant::BOOL, "held")));
    ADD_SIGNAL(MethodInfo("spawn_requested",
            PropertyInfo(Variant::STRING, "kind"),
            PropertyInfo(Variant::VECTOR3, "position"),
            PropertyInfo(Variant::DICTIONARY, "data")));
}

void BossController3D::_ready() {
    add_to_group(StringName("enemies"));
    add_to_group(StringName("bosses"));

    sprite = Object::cast_to<Sprite3D>(get_node_or_null(NodePath("Sprite")));
    telegraph_visual = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("Telegraph")));
    melee_telegraph = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("MeleeTelegraph")));
    visual = get_node_or_null(NodePath("Visual"));

    health = max_health;
    set_motion_mode(CharacterBody3D::MOTION_MODE_FLOATING);
    hide_telegraphs();
}

// ===================================================================== утилиты

Node3D *BossController3D::get_player() const {
    if (!is_inside_tree()) return nullptr;
    return find_player_3d(this);
}

int BossController3D::count_group(const char *name) const {
    if (!is_inside_tree()) return 0;
    return int(get_tree()->get_nodes_in_group(StringName(name)).size());
}

double BossController3D::player_stun_left(Node3D *player) const {
    if (player == nullptr) return 0.0;
    if (!player->has_method(StringName("get_stun_left"))) return 0.0;
    return double(player->call(StringName("get_stun_left")));
}

double BossController3D::cd_scale() const {
    double base = phase == 3 ? 0.45 : (phase == 2 ? 0.7 : 1.0);
    if (fight_time > 60.0) base *= 0.85;
    if (fight_time > 120.0) base *= 0.8;
    return base;
}

Vector3 BossController3D::random_arena_point(double margin) const {
    const double x = UtilityFunctions::randf_range(-(arena_half.x - margin), arena_half.x - margin);
    const double z = UtilityFunctions::randf_range(-(arena_half.y - margin), arena_half.y - margin);
    return Vector3(x, 0.0, z);
}

Vector3 BossController3D::point_near(const Vector3 &center, double min_r, double max_r, double margin) const {
    const double angle = UtilityFunctions::randf_range(0.0, 6.2831853);
    const double r = UtilityFunctions::randf_range(min_r, max_r);
    Vector3 p = center + Vector3(std::cos(angle) * r, 0.0, std::sin(angle) * r);

    const real_t min_x = real_t(-(arena_half.x - margin));
    const real_t max_x = real_t(arena_half.x - margin);
    const real_t min_z = real_t(-(arena_half.y - margin));
    const real_t max_z = real_t(arena_half.y - margin);

    p.x = std::clamp(p.x, min_x, max_x);
    p.z = std::clamp(p.z, min_z, max_z);
    p.y = 0.0;
    return p;
}

Vector3 BossController3D::random_far_point(const Vector3 &from, double min_distance, double margin) const {
    Vector3 best = random_arena_point(margin);
    double best_distance = 0.0;
    for (int i = 0; i < 14; ++i) {
        const Vector3 candidate = random_arena_point(margin);
        Vector3 d = candidate - from;
        d.y = 0.0;
        const double dist = d.length();
        if (dist >= min_distance) return candidate;
        if (dist > best_distance) {
            best_distance = dist;
            best = candidate;
        }
    }
    return best;
}

void BossController3D::request_spawn(const String &kind, const Vector3 &position, const Dictionary &data) {
    emit_signal(StringName("spawn_requested"), kind, position, data);
}

void BossController3D::hide_telegraphs() {
    if (telegraph_visual != nullptr) {
        telegraph_visual->set_visible(false);
        telegraph_visual->set_position(Vector3(0.0, 0.05, 0.0));
    }
    if (melee_telegraph != nullptr) melee_telegraph->set_visible(false);
}

// ===================================================================== движение и интеллект

bool BossController3D::is_cornered() const {
    const double margin = 1.8;
    const Vector3 p = get_global_position();
    return std::fabs(p.x) > arena_half.x - margin || std::fabs(p.z) > arena_half.y - margin;
}

Vector3 BossController3D::escape_corner_point() const {
    Vector3 to_center = -get_global_position();
    to_center.y = 0.0;
    if (to_center.length_squared() < 0.001) return Vector3();
    return to_center.normalized() * (arena_half.x * 0.45);
}

bool BossController3D::player_is_dangerous(Node3D *player) const {
    if (player == nullptr) return false;
    // Стан = игрок не опасен, наоборот — это окно расправы.
    if (player_stun_left(player) > 0.15) return false;
    if (player->has_method(StringName("is_dashing")) &&
            bool(player->call(StringName("is_dashing")))) return true;
    if (player->has_method(StringName("get_dash_cooldown_left"))) {
        if (double(player->call(StringName("get_dash_cooldown_left"))) <= 0.25) return true;
    }
    if (recent_player_hits >= 3) return true;
    return false;
}

void BossController3D::update_movement(double delta, Node3D *player) {
    strafe_change_left -= delta;
    if (strafe_change_left <= 0.0) {
        strafe_dir = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
        strafe_change_left = 1.2 + UtilityFunctions::randf() * 1.5;
    }
    corner_escape_cooldown = std::max(0.0, corner_escape_cooldown - delta);

    const bool rooted = count_group("roots") > 0;
    Vector3 desired;

    if (rooted || player == nullptr) {
        set_velocity(desired);
        move_and_slide();
        return;
    }

    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    const double dist = to_player.length();
    const Vector3 dir = (dist > 0.001) ? to_player / dist : Vector3(1, 0, 0);

    const double stun_left = player_stun_left(player);

    // Приоритет 0: игрок в стане — бежим в мили, не отвлекаемся.
    if (stun_left > 0.15 && spruce_state == SPRUCE_HELD) {
        if (dist > MELEE_PREFERRED) {
            Vector3 side = dir.cross(Vector3(0, 1, 0)).normalized() * strafe_dir * 0.15;
            desired = (dir + side).normalized() * move_speed * 1.35;
        }
        set_velocity(desired);
        move_and_slide();
        return;
    }

    // Приоритет 1: escape from corner
    if (is_cornered() && corner_escape_cooldown <= 0.0) {
        move_intent = MOVE_TO_POINT;
        move_target = escape_corner_point();
        corner_escape_cooldown = 1.5;
    }
    // Приоритет 2: подбор ели
    else if (spruce_state == SPRUCE_GROUND) {
        Vector3 to_spruce = spruce_pos - get_global_position();
        to_spruce.y = 0.0;
        if (to_spruce.length() <= SPRUCE_PICKUP_RADIUS) {
            Array pickups = get_tree()->get_nodes_in_group(StringName("spruce_pickup"));
            for (int i = 0; i < pickups.size(); ++i) {
                Node *n = Object::cast_to<Node>(pickups[i]);
                if (n != nullptr) n->queue_free();
            }
            spruce_state = SPRUCE_HELD;
            emit_signal(StringName("spruce_changed"), true);
            recover_left = 0.5;
        } else {
            desired = to_spruce.normalized() * (move_speed * 1.5);
        }
        set_velocity(desired);
        move_and_slide();
        return;
    }
    // Приоритет 3: ель летит
    else if (spruce_state == SPRUCE_FLYING) {
        if (dist < 4.0) desired = -dir * move_speed * 0.9;
        else if (dist > 6.0) {
            Vector3 side = dir.cross(Vector3(0, 1, 0)).normalized() * strafe_dir;
            desired = side * move_speed * 0.6;
        }
        set_velocity(desired);
        move_and_slide();
        return;
    }
    // Приоритет 4: обычный бой
    else if (spruce_state == SPRUCE_HELD) {
        const bool danger = player_is_dangerous(player);
        if (dist < MELEE_PREFERRED - 0.6 && danger) move_intent = MOVE_RETREAT;
        else if (dist > MELEE_PREFERRED + 1.2)       move_intent = MOVE_APPROACH;
        else                                          move_intent = MOVE_STRAFE;
    }

    switch (move_intent) {
        case MOVE_APPROACH: {
            Vector3 side = dir.cross(Vector3(0, 1, 0)).normalized() * strafe_dir * 0.35;
            desired = (dir + side).normalized() * move_speed;
        } break;
        case MOVE_RETREAT: {
            Vector3 side = dir.cross(Vector3(0, 1, 0)).normalized() * strafe_dir * 0.8;
            desired = (-dir + side).normalized() * move_speed * 0.95;
        } break;
        case MOVE_STRAFE: {
            Vector3 side = dir.cross(Vector3(0, 1, 0)).normalized() * strafe_dir;
            desired = side * move_speed * 0.65;
        } break;
        case MOVE_TO_POINT: {
            Vector3 to_t = move_target - get_global_position();
            to_t.y = 0.0;
            if (to_t.length() > 0.6) desired = to_t.normalized() * move_speed * 1.15;
            else move_intent = MOVE_HOLD;
        } break;
        default: break;
    }

    set_velocity(desired);
    move_and_slide();
}

// ===================================================================== punish / threat

void BossController3D::register_player_hit(double distance) {
    if (distance > MELEE_RANGE + 0.5) return;
    const double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
    if (now - last_player_attack_time < 0.85) recent_player_hits++;
    else recent_player_hits = 1;
    last_player_attack_time = now;
}

bool BossController3D::should_punish() const {
    const double now = Time::get_singleton()->get_ticks_msec() / 1000.0;
    return recent_player_hits >= 2
        && (now - last_punish_time) > 2.5
        && cd_punish <= 0.0;
}

void BossController3D::trigger_punish() {
    Node3D *player = get_player();
    if (player == nullptr) return;

    Vector3 ppos = player->get_global_position();
    ppos.y = 0.0;

    const int root_count = (phase >= 3) ? 4 : 2;
    for (int i = 0; i < root_count; ++i) {
        Vector3 pos = ppos + Vector3(
            UtilityFunctions::randf_range(-1.6, 1.6), 0.0,
            UtilityFunctions::randf_range(-1.6, 1.6));
        Dictionary d;
        d["kind"] = String("root");
        d["radius"] = 1.0;
        d["telegraph"] = 0.35;
        d["active"] = 3.0;
        d["damage"] = 16.0 * damage_scale;
        d["stun"] = 1.0;
        request_spawn("root", pos, d);
    }

    recent_player_hits = 0;
    last_punish_time = Time::get_singleton()->get_ticks_msec() / 1000.0;
    cd_punish = 5.0;
    recover_left = 0.35;
}

bool BossController3D::should_attack_now(Node3D *player, double dist) const {
    if (player == nullptr) return false;

    if (player_stun_left(player) > 0.2 && dist <= MELEE_RANGE - 0.2 && spruce_state == SPRUCE_HELD) return true;

    const bool no_dash = player->has_method(StringName("get_dash_cooldown_left")) &&
                        double(player->call(StringName("get_dash_cooldown_left"))) > 0.25;
    if (no_dash && dist <= MELEE_RANGE - 0.2 && spruce_state == SPRUCE_HELD) return true;
    if (dist >= 4.0 && dist <= 9.0 && cd_throw <= 0.0) return true;
    return false;
}

void BossController3D::spawn_root_wall(const Vector3 &center, const Vector3 &away_dir) {
    Vector3 perp = away_dir.cross(Vector3(0, 1, 0)).normalized();
    if (perp.length_squared() < 0.01) perp = Vector3(1, 0, 0);

    for (int i = -1; i <= 1; ++i) {
        Vector3 pos = center + perp * (i * 1.4f) + away_dir * 1.5f;
        pos.y = 0.0;
        pos.x = std::clamp(pos.x, -(real_t)(arena_half.x - 0.5), real_t(arena_half.x - 0.5));
        pos.z = std::clamp(pos.z, -(real_t)(arena_half.y - 0.5), real_t(arena_half.y - 0.5));

        Dictionary d;
        d["kind"] = String("root");
        d["radius"] = 0.85;
        d["telegraph"] = 0.5;
        d["active"] = 4.0;
        d["damage"] = 14.0 * damage_scale;
        d["stun"] = 0.9;
        request_spawn("root", pos, d);
    }
}

// ===================================================================== основной цикл

void BossController3D::_physics_process(double delta) {
    fight_time += delta;
    time_since_player_hit += delta;
    hit_flash_left = std::max(0.0, hit_flash_left - delta);
    cd_punish = std::max(0.0, cd_punish - delta);

    update_phase();
    tick_timers(delta);

    Node3D *player = get_player();
    Vector3 to_player;
    double dist = 0.0;
    if (player != nullptr) {
        to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
        dist = to_player.length();
        cached_player_distance = dist;

        // Оценка скорости игрока (сглаженная).
        const double now_time = Time::get_singleton()->get_ticks_msec() / 1000.0;
        if (last_player_sample_time > 0.0) {
            const double dt = now_time - last_player_sample_time;
            if (dt > 0.001 && dt < 0.25) {
                Vector3 v = (player->get_global_position() - last_player_pos) / dt;
                v.y = 0.0;
                player_velocity_estimate = player_velocity_estimate.lerp(v, 0.35);
            }
        }
        last_player_pos = player->get_global_position();
        last_player_sample_time = now_time;
    }
    update_sprite(to_player);
    if (player != nullptr) push_player_out(player, to_player, delta);

    if (spruce_state == SPRUCE_FLYING) {
        spruce_fly_left -= delta;
        if (spruce_fly_left <= 0.0) {
            spruce_state = SPRUCE_GROUND;
            spruce_pos = action_target;
        }
    }

    // --- Замах / восстановление ---
    if (action != ACTION_NONE) {
        windup_left -= delta;
        if (windup_left <= 0.0) execute_action(player);
        set_velocity(Vector3());
        move_and_slide();
        return;
    }

    if (recover_left > 0.0) {
        recover_left -= delta;
        set_velocity(Vector3());
        move_and_slide();
        return;
    }

    // ============================================================
    // РЕАКТИВНЫЙ ИНСТИНКТ: игрок вошёл в стан — бьём вне очереди.
    // ============================================================
    if (player != nullptr) {
        const double stun_left = player_stun_left(player);
        if (stun_left > 0.15) {
            if (!stun_attack_fired) {
                stun_attack_fired = true;
                if (spruce_state == SPRUCE_HELD) {
                    if (dist <= MELEE_RANGE - 0.2) {
                        begin_action(ACTION_MELEE, player);
                        return;
                    } else if (cd_throw <= 0.0) {
                        begin_action(ACTION_THROW_SPRUCE, player);
                        return;
                    }
                } else {
                    if (cd_mushrooms <= 0.0) {
                        begin_action(ACTION_MUSHROOMS, player);
                        return;
                    } else if (cd_roots <= 0.0 && count_group("roots") == 0) {
                        begin_action(ACTION_ROOTS, player);
                        return;
                    }
                }
            }
        } else {
            stun_attack_fired = false;
        }
    }

    // --- Комбо-окно ---
    if (combo_window_left > 0.0) {
        combo_window_left -= delta;
        if (player != nullptr) {
            choose_action(player);
            return;
        }
    }

    // --- Punish: игрок спамит в упор ---
    if (should_punish() && player != nullptr) {
        trigger_punish();
        set_velocity(Vector3());
        move_and_slide();
        return;
    }

    // ============================================================
    // АНТИ-ЗАСТОЙ: игрок долго стоит в мили — ШВЫРЯЕМ его к стене.
    // ============================================================
    if (player != nullptr) {
        const bool player_dashing_now = player->has_method(StringName("is_dashing")) &&
                                        bool(player->call(StringName("is_dashing")));
        const bool player_stunned_now = player_stun_left(player) > 0.1;
        const bool in_melee_zone = dist < MELEE_RANGE + 0.4;
        const bool can_be_pressured = in_melee_zone && !player_dashing_now && !player_stunned_now
                                      && spruce_state == SPRUCE_HELD;

        if (can_be_pressured) {
            melee_pressure_time += delta;
        } else {
            // Быстрый сброс — игрок либо ушёл, либо активно уклонился.
            melee_pressure_time = std::max(0.0, melee_pressure_time - delta * 3.0);
        }

        if (melee_pressure_time >= MELEE_PRESSURE_LIMIT
                && cd_throw_player <= 0.0
                && spruce_state == SPRUCE_HELD) {
            begin_action(ACTION_GRAB_THROW, player);
            melee_pressure_time = 0.0;
            cd_throw_player = THROW_PLAYER_CD;
            return;
        }
    }

    // --- Анти-кайт ---
    if (player != nullptr && time_since_player_hit > 4.5 && phase >= 2
            && cd_birds <= 0.0 && count_group("minions") < MAX_MINIONS) {
        Vector3 ppos = player->get_global_position();
        ppos.y = 0.0;
        for (int i = 0; i < 2; ++i) {
            Vector3 pos = ppos + Vector3(UtilityFunctions::randf_range(-3, 3), 0.0,
                                          UtilityFunctions::randf_range(-3, 3));
            Dictionary d;
            d["kind"] = String("birds");
            d["radius"] = 2.3;
            d["telegraph"] = 0.9;
            d["active"] = 4.0;
            d["damage"] = 10.0 * damage_scale;
            d["tick"] = 0.5;
            request_spawn("birds", pos, d);
        }
        Vector3 mpos = point_near(ppos, 2.5, 3.5, 0.9);
        Dictionary md;
        md["kind"] = String("minion");
        request_spawn("minion", mpos, md);

        cd_birds = 6.0 * cd_scale();
        cd_summon = 8.0 * cd_scale();
        time_since_player_hit = 0.0;
    }

    // --- Движение ---
    update_movement(delta, player);

    // --- Выбор следующей способности ---
    decision_left -= delta;
    if (decision_left <= 0.0 && player != nullptr) {
        if (should_attack_now(player, dist) || UtilityFunctions::randf() < 0.35) {
            choose_action(player);
        } else {
            decision_left = 0.15;
        }
    }
}

void BossController3D::tick_timers(double delta) {
    cd_roots = std::max(0.0, cd_roots - delta);
    cd_birds = std::max(0.0, cd_birds - delta);
    cd_summon = std::max(0.0, cd_summon - delta);
    cd_mushrooms = std::max(0.0, cd_mushrooms - delta);
    cd_throw = std::max(0.0, cd_throw - delta);
    cd_shrink = std::max(0.0, cd_shrink - delta);
    cd_throw_player = std::max(0.0, cd_throw_player - delta);
}

void BossController3D::update_phase() {
    if (max_health <= 0.0) return;
    const double ratio = health / max_health;
    const int old_phase = phase;

    if (ratio > 0.66)      { phase = 1; move_speed = 1.3; damage_scale = 1.0;  }
    else if (ratio > 0.33) { phase = 2; move_speed = 1.7; damage_scale = 1.35; }
    else                   { phase = 3; move_speed = 2.1; damage_scale = 1.7;  }

    if (old_phase == phase || phase == 1) return;

    action = ACTION_NONE;
    windup_left = 0.0;
    recover_left = 0.8;
    decision_left = 0.6;
    cd_roots = 0.0; cd_birds = 0.0; cd_mushrooms = 0.0;

    Node3D *player = get_player();
    if (player == nullptr) return;

    Vector3 ppos = player->get_global_position();
    ppos.y = 0.0;

    const int ring = (phase == 2) ? 5 : 7;
    for (int i = 0; i < ring; ++i) {
        const float a = i * (6.2832f / ring);
        Vector3 offset(std::cos(a) * 2.8f, 0.0, std::sin(a) * 2.8f);
        Dictionary d;
        d["kind"] = String("root");
        d["radius"] = 0.9;
        d["telegraph"] = 0.55;
        d["active"] = 3.5;
        d["damage"] = 12.0 * damage_scale;
        d["stun"] = 0.6;
        request_spawn("root", ppos + offset, d);
    }

    if (phase == 3) {
        decision_left = 0.1;
        cd_roots = 0.0; cd_birds = 0.0; cd_mushrooms = 0.0;
        cd_summon = 0.0; cd_throw = 0.0;
        combo_depth = 0;
        rage_step = 0;
    }
}

// ===================================================================== выбор способности

void BossController3D::choose_action(Node3D *player) {
    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    const double dist = to_player.length();
    const bool stunned = player_stun_left(player) > 0.1;
    const bool held = (spruce_state == SPRUCE_HELD);

    // --- Фаза 3: детерминированная связка ---
    if (phase == 3 && combo_depth == 0) {
        switch (rage_step % 3) {
            case 0: {
                if (cd_birds > 0.0) cd_birds = 0.0;
                begin_action(ACTION_BIRDS, player);
                break;
            }
            case 1: {
                if (cd_roots > 0.0) cd_roots = 0.0;
                begin_action(ACTION_ROOTS, player);
                break;
            }
            case 2: {
                if (dist < MELEE_RANGE && held) begin_action(ACTION_MELEE, player);
                else if (cd_throw <= 0.0 && held) begin_action(ACTION_THROW_SPRUCE, player);
                else begin_action(ACTION_MUSHROOMS, player);
                break;
            }
        }
        rage_step++;
        return;
    }

    double w[ACTION_COUNT];
    for (int i = 0; i < ACTION_COUNT; ++i) w[i] = 0.0;

    if (held) {
        if (dist <= MELEE_RANGE - 0.3) w[ACTION_MELEE] = stunned ? 20.0 : 8.0;
        if (dist >= 3.6 && cd_throw <= 0.0) w[ACTION_THROW_SPRUCE] = stunned ? 12.0 : 3.5;
    }
    if (count_group("roots") == 0 && cd_roots <= 0.0) w[ACTION_ROOTS] = 4.0;
    if (cd_birds <= 0.0) w[ACTION_BIRDS] = 2.8;
    if (count_group("minions") < MAX_MINIONS - 1 && cd_summon <= 0.0) w[ACTION_SUMMON] = 2.4;
    if (cd_mushrooms <= 0.0) w[ACTION_MUSHROOMS] = stunned ? 6.0 : 3.0;
    if (phase >= 2 && cd_shrink <= 0.0) w[ACTION_SHRINK] = 2.0;

    // ---- Smart adjustments ----
    const bool player_dashing = player->has_method(StringName("is_dashing")) &&
                                bool(player->call(StringName("is_dashing")));

    if (player_dashing && cd_roots <= 0.0) w[ACTION_ROOTS] += 5.0;

    if (stunned) {
        w[ACTION_MELEE] *= 2.5;
        w[ACTION_THROW_SPRUCE] *= 2.0;
        w[ACTION_MUSHROOMS] *= 1.5;
    }

    if (dist > 7.0) {
        w[ACTION_BIRDS] *= 1.6;
        w[ACTION_SUMMON] *= 1.4;
        if (!player_dashing) w[ACTION_ROOTS] *= 0.5;
    }

    if (phase == 3 && dist < 4.5) {
        if (cd_shrink <= 0.0) w[ACTION_SHRINK] += 3.0;
        if (cd_roots <= 0.0)  w[ACTION_ROOTS] += 2.0;
    }

    // Anti-repetition — кроме стана: там повторный удар только в кайф.
    if (!stunned) {
        if (last_action == ACTION_BIRDS)     w[ACTION_BIRDS] *= 0.35;
        if (last_action == ACTION_ROOTS)     w[ACTION_ROOTS] *= 0.35;
        if (last_action == ACTION_MUSHROOMS) w[ACTION_MUSHROOMS] *= 0.45;
        if (last_action == ACTION_SUMMON)    w[ACTION_SUMMON] *= 0.3;
        if (last_action == ACTION_MELEE)     w[ACTION_MELEE] *= 0.5;
    }

    double total = 0.0;
    for (int i = 0; i < ACTION_COUNT; ++i) total += w[i];
    if (total <= 0.0) {
        decision_left = 0.35;
        return;
    }

    double pick = UtilityFunctions::randf() * total;
    for (int i = 1; i < ACTION_COUNT; ++i) {
        if (w[i] <= 0.0) continue;
        pick -= w[i];
        if (pick <= 0.0) {
            begin_action(static_cast<Action>(i), player);
            return;
        }
    }
    begin_action(ACTION_BIRDS, player);
}

void BossController3D::begin_action(Action a, Node3D *player) {
    action = a;

    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    if (to_player.length_squared() > 0.0001) action_dir = to_player.normalized();

    switch (a) {
        case ACTION_MELEE:
            if (player_stun_left(player) > 0.15) {
                windup_total = 0.5;
            } else {
                windup_total = (UtilityFunctions::randf() < 0.25) ? 1.05 : 0.75;
            }
            if (melee_telegraph != nullptr) {
                melee_telegraph->set_rotation(Vector3(0.0, std::atan2(-action_dir.z, action_dir.x), 0.0));
                melee_telegraph->set_visible(true);
            }
            break;

        case ACTION_THROW_SPRUCE: {
            windup_total = 0.9;
            const bool stunned = player_stun_left(player) > 0.15;
            const Vector3 lead = stunned ? Vector3() : player_velocity_estimate * 0.75;
            action_target = player->get_global_position() + lead;
            action_target.x = std::clamp(action_target.x,
                    -(real_t)(arena_half.x - 0.5), real_t(arena_half.x - 0.5));
            action_target.z = std::clamp(action_target.z,
                    -(real_t)(arena_half.y - 0.5), real_t(arena_half.y - 0.5));
            action_target.y = 0.0;
            if (telegraph_visual != nullptr) {
                telegraph_visual->set_global_position(action_target + Vector3(0.0, 0.05, 0.0));
                telegraph_visual->set_scale(Vector3(SPRUCE_RADIUS, 1.0, SPRUCE_RADIUS));
                telegraph_visual->set_visible(true);
            }
        } break;

        case ACTION_ROOTS: windup_total = 0.8; break;
        case ACTION_BIRDS: windup_total = 0.7; break;
        case ACTION_SUMMON: windup_total = 1.0; break;

        case ACTION_MUSHROOMS:
            windup_total = 0.7;
            mushroom_throw_mode = (phase >= 2) && (UtilityFunctions::randf() < 0.5);
            break;

        case ACTION_GRAB_THROW: {
            windup_total = THROW_PLAYER_WINDUP;
            if (telegraph_visual != nullptr) {
                telegraph_visual->set_global_position(get_global_position() + Vector3(0.0, 0.05, 0.0));
                telegraph_visual->set_scale(Vector3(MELEE_RANGE, 1.0, MELEE_RANGE));
                telegraph_visual->set_visible(true);
            }
        } break;

        case ACTION_SHRINK: windup_total = 1.2; break;
        default: windup_total = 0.5; break;
    }
    windup_left = windup_total;
}

// ===================================================================== применение способности

void BossController3D::execute_action(Node3D *player) {
    const Vector3 self_pos = get_global_position();
    const Action finished = action;
    double recover = 0.5;

    hide_telegraphs();

    switch (finished) {
        case ACTION_MELEE: {
            if (player != nullptr) {
                Vector3 to_player = player->get_global_position() - self_pos;
                to_player.y = 0.0;
                const double dist = to_player.length();
                const double cos_threshold = (player_stun_left(player) > 0.1)
                                             ? 0.0 : MELEE_COS_HALF_ANGLE;
                if (dist <= MELEE_RANGE && dist > 0.001 &&
                        to_player.normalized().dot(action_dir) >= cos_threshold &&
                        player->has_method(StringName("take_damage"))) {
                    player->call(StringName("take_damage"), MELEE_DAMAGE * damage_scale);
                }
            }
            recover = 0.55;
        } break;

        case ACTION_THROW_SPRUCE: {
            Dictionary d;
            d["kind"] = String("spruce");
            d["arc"] = true;
            d["from"] = self_pos + Vector3(1.6, 2.6, 0.0);
            d["to"] = action_target;
            d["flight_time"] = 0.75;
            d["arc_height"] = 2.4;
            d["damage"] = SPRUCE_DAMAGE * damage_scale;
            d["radius"] = SPRUCE_RADIUS;
            request_spawn("projectile", action_target, d);
            spruce_state = SPRUCE_FLYING;
            spruce_fly_left = 3.0;
            emit_signal(StringName("spruce_changed"), false);
            cd_throw = 7.0 * cd_scale();
            recover = 0.6;
        } break;

        case ACTION_ROOTS: {
            const int count = (phase == 1) ? 4 : (phase == 2 ? 6 : 8);
            const Vector3 target = (player != nullptr) ? player->get_global_position() : self_pos;
            for (int i = 0; i < count; ++i) {
                const Vector3 pos = (i % 5 < 3) ? point_near(target, 1.6, 4.0, 0.9)
                                                 : random_arena_point(0.9);
                Dictionary d;
                d["kind"] = String("root");
                d["radius"] = 0.95;
                d["telegraph"] = 0.8;
                d["active"] = 6.0;
                d["damage"] = 18.0 * damage_scale;
                d["stun"] = 1.2;
                request_spawn("root", pos, d);
            }
            if (player != nullptr && phase >= 2) {
                Vector3 to_p = player->get_global_position() - self_pos;
                to_p.y = 0.0;
                if (to_p.length() > 0.001) {
                    spawn_root_wall(player->get_global_position(), to_p.normalized());
                }
            }
            cd_roots = 6.0 * cd_scale();
            recover = 0.5;
        } break;

        case ACTION_BIRDS: {
            const int count = (phase == 1) ? 1 : (phase == 2 ? 2 : 3);
            for (int i = 0; i < count; ++i) {
                const Vector3 pos = random_far_point(self_pos, 5.0, 2.2);
                Dictionary d;
                d["kind"] = String("birds");
                d["radius"] = 2.3;
                d["telegraph"] = 0.9;
                d["active"] = 4.0;
                d["damage"] = 10.0 * damage_scale;
                d["tick"] = 0.5;
                request_spawn("birds", pos, d);
            }
            cd_birds = 6.0 * cd_scale();
            recover = 0.5;
        } break;

        case ACTION_SUMMON: {
            int count = (phase == 1) ? 2 : 3;
            count = std::min(count, MAX_MINIONS - count_group("minions"));
            for (int i = 0; i < count; ++i) {
                const Vector3 pos = point_near(self_pos, 2.0, 2.6, 0.8);
                Dictionary d;
                d["kind"] = String("minion");
                request_spawn("minion", pos, d);
            }
            cd_summon = 10.0 * cd_scale();
            recover = 0.6;
        } break;

        case ACTION_MUSHROOMS: {
            const Vector3 target = (player != nullptr) ? player->get_global_position() : self_pos;
            if (mushroom_throw_mode) {
                const bool stunned = (player != nullptr) && player_stun_left(player) > 0.15;
                const Vector3 lead = stunned ? Vector3() : player_velocity_estimate * 0.8;
                Dictionary d;
                d["kind"] = String("mushroom");
                d["arc"] = true;
                d["from"] = self_pos + Vector3(0.0, 3.0, 0.0);
                d["to"] = target + lead;
                d["flight_time"] = 0.9;
                d["arc_height"] = 3.0;
                request_spawn("projectile", target, d);
            } else {
                const int count = (phase == 1) ? 3 : (phase == 2 ? 4 : 5);
                for (int i = 0; i < count; ++i) {
                    const Vector3 pos = (i == 0) ? target : point_near(target, 1.5, 4.0, 0.9);
                    on_projectile_landed("mushroom", pos);
                }
            }
            cd_mushrooms = 5.0 * cd_scale();
            recover = 0.5;
        } break;

        case ACTION_GRAB_THROW: {
            if (player != nullptr) {
                Vector3 to_p = player->get_global_position() - self_pos;
                to_p.y = 0.0;
                const double d = to_p.length();
                const double catch_range = MELEE_RANGE + 0.6;

                if (d <= catch_range) {
                    Vector3 push_dir = (d > 0.001) ? (to_p / d) : action_dir;

                    const double wall_margin = 0.6;
                    Vector3 target = self_pos + push_dir * ((arena_half.x + arena_half.y) * 2.0);
                    target.x = std::clamp(target.x,
                            -(real_t)(arena_half.x - wall_margin),
                            real_t(arena_half.x - wall_margin));
                    target.z = std::clamp(target.z,
                            -(real_t)(arena_half.y - wall_margin),
                            real_t(arena_half.y - wall_margin));
                    target.y = 0.0;

                    Vector3 throw_dir = target - player->get_global_position();
                    throw_dir.y = 0.0;
                    const double throw_len = throw_dir.length();

                    if (throw_len > 0.1) {
                        throw_dir = throw_dir / throw_len;

                        if (player->has_method(StringName("take_damage"))) {
                            player->call(StringName("take_damage"),
                                    THROW_PLAYER_DAMAGE * damage_scale);
                        }
                        if (player->has_method(StringName("apply_stun"))) {
                            player->call(StringName("apply_stun"), THROW_PLAYER_STUN);
                        }
                        if (player->has_method(StringName("apply_knockback"))) {
                            player->call(StringName("apply_knockback"),
                                    throw_dir * THROW_PLAYER_IMPULSE);
                        }
                    }
                }
            }
            recover = 0.9;
        } break;

        case ACTION_SHRINK: {
            Dictionary d;
            d["duration"] = (phase == 3) ? 12.0 : 9.0;
            d["scale"] = 0.62;
            request_spawn("shrink_arena", Vector3(), d);
            cd_shrink = 22.0 * cd_scale();
            recover = 0.7;
        } break;

        default: break;
    }

    // ---- Комбо-связка ----
    bool hit_confirmed = false;
    if (finished == ACTION_MELEE && player != nullptr) {
        Vector3 d = player->get_global_position() - self_pos;
        d.y = 0.0;
        if (d.length() <= MELEE_RANGE) hit_confirmed = true;
    }
    if (finished == ACTION_THROW_SPRUCE || finished == ACTION_MUSHROOMS) hit_confirmed = true;
    if (finished == ACTION_ROOTS || finished == ACTION_BIRDS) hit_confirmed = true;

    const bool player_still_stunned = (player != nullptr) && player_stun_left(player) > 0.1;

    // grab_throw — это не часть комбо-цепочки, у него своя роль.
    if (finished == ACTION_GRAB_THROW) {
        combo_depth = 0;
        combo_window_left = 0.0;
    } else if ((hit_confirmed || player_still_stunned) && phase >= 2 && combo_depth < 2) {
        combo_depth += 1;
        combo_window_left = 0.35;
        recover *= 0.35;
    } else {
        combo_depth = 0;
    }

    if (phase == 2) recover *= 0.75;
    else if (phase == 3) recover *= 0.5;

    last_action = finished;
    action = ACTION_NONE;
    recover_left = recover;
    decision_left = (phase == 1) ? 1.0 : (phase == 2 ? 0.55 : 0.25);
}

void BossController3D::on_projectile_landed(const String &kind, const Vector3 &position) {
    if (kind == String("spruce")) {
        spruce_state = SPRUCE_GROUND;
        spruce_pos = position;
    } else if (kind == String("mushroom")) {
        Dictionary d;
        d["kind"] = String("mushroom");
        d["radius"] = 2.2;
        d["telegraph"] = 1.2;
        d["active"] = 0.45;
        d["damage"] = 20.0 * damage_scale;
        d["explodes"] = true;
        d["slow_factor"] = 0.65;
        d["slow_time"] = 4.0;
        d["weaken_mult"] = 0.7;
        d["weaken_time"] = 6.0;
        request_spawn("mushroom", position, d);
    }
}

// ===================================================================== визуал и прочее

void BossController3D::update_sprite(const Vector3 &to_player) {
    if (to_player.length_squared() > 0.01) facing_dir = to_player.normalized();
    if (sprite != nullptr && std::fabs(to_player.x) > 0.3) sprite->set_flip_h(to_player.x < 0.0);

    Color tint = Color(1.0, 1.0, 1.0, 1.0);
    if (phase == 2) tint = Color(1.0, 0.82, 0.74, 1.0);
    else if (phase == 3) tint = Color(1.0, 0.58, 0.52, 1.0);

    if (action != ACTION_NONE) {
        const double pulse = 0.5 + 0.5 * std::sin(windup_left * 28.0);
        if (action == ACTION_MELEE || action == ACTION_THROW_SPRUCE) {
            tint = Color(1.0 + 0.35 * pulse, 0.75, 0.7, 1.0);
        } else if (action == ACTION_GRAB_THROW) {
            tint = Color(1.0 + 0.5 * pulse, 0.5 + 0.2 * pulse, 0.9, 1.0);
        } else {
            tint = Color(0.75, 1.0 + 0.3 * pulse, 0.75, 1.0);
        }
    }

    if (hit_flash_left > 0.0) tint = Color(tint.r * 1.5, tint.g * 1.5, tint.b * 1.5, 1.0);
    if (sprite != nullptr) sprite->set_modulate(tint);
    if (visual != nullptr && visual->has_method(StringName("set_tint"))) {
        visual->call(StringName("set_tint"), tint);
    }
}

void BossController3D::take_damage(double amount) {
    if (amount <= 0.0 || health <= 0.0) return;

    Node3D *player = get_player();
    if (player != nullptr) {
        Vector3 d = player->get_global_position() - get_global_position();
        d.y = 0.0;
        register_player_hit(d.length());
    }

    health = std::max(0.0, health - amount);
    hit_flash_left = 0.07;
    time_since_player_hit = 0.0;
    if (health <= 0.0) {
        emit_signal(StringName("defeated"));
        queue_free();
    }
}

void BossController3D::apply_knockback(const Vector3 &) {}

void BossController3D::set_arena_half(const Vector2 &half) { arena_half = half; }

void BossController3D::push_player_out(Node3D *player, const Vector3 &to_player, double delta) {
    // Односторонняя "твёрдость": вытесняется только герой, босс остаётся на месте.
    // (Раньше слой героя совпадал с маской босса, и move_and_slide() выталкивал самого босса.)
    if (!player->has_method(StringName("push_out"))) return;
    const double min_dist = BODY_PUSH_RADIUS + PLAYER_BODY_RADIUS;
    const double dist = to_player.length();
    if (dist >= min_dist) return;
    const Vector3 dir = (dist > 0.001) ? (to_player / dist) : Vector3(0.0, 0.0, 1.0);
    const double amount = (min_dist - dist) * std::min(1.0, delta * BODY_PUSH_STIFFNESS);
    player->call(StringName("push_out"), dir * amount);
}

double BossController3D::get_windup_progress() const {
    if (action == ACTION_NONE || windup_total <= 0.0) return 0.0;
    return std::clamp(1.0 - windup_left / windup_total, 0.0, 1.0);
}

Vector3 BossController3D::get_facing_dir() const { return facing_dir; }

double BossController3D::get_health() const { return health; }
double BossController3D::get_max_health() const { return max_health; }
double BossController3D::get_hit_radius() const { return 1.3; }
int BossController3D::get_phase() const { return phase; }
bool BossController3D::is_spruce_held() const { return spruce_state == SPRUCE_HELD; }

String BossController3D::get_spruce_state_name() const {
    switch (spruce_state) {
        case SPRUCE_HELD: return "held";
        case SPRUCE_FLYING: return "flying";
        default: return "ground";
    }
}

String BossController3D::get_action_name() const {
    switch (action) {
        case ACTION_MELEE: return "melee";
        case ACTION_THROW_SPRUCE: return "throw";
        case ACTION_ROOTS: return "roots";
        case ACTION_BIRDS: return "birds";
        case ACTION_SUMMON: return "summon";
        case ACTION_MUSHROOMS: return "mushrooms";
        case ACTION_SHRINK: return "shrink";
        case ACTION_GRAB_THROW: return "grab_throw";
        default: return "none";
    }
}
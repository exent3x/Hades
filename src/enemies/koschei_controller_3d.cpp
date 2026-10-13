#include <algorithm>
#include <cmath>

#include "koschei_controller_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// ----- Баланс (метры / секунды). Леший: 900 HP, урон 24–32. Кощей заметно сильнее. -----
static const double SWIPE_DAMAGE = 26.0;
static const double SWIPE_REACH = 3.9;
static const double SWIPE_COS = 0.40;
static const double LUNGE_DAMAGE = 32.0;
static const double LUNGE_SPEED = 26.0;
static const double LUNGE_MAX = 8.5;
static const double SLAM_DAMAGE = 30.0;
static const double SLAM_RADIUS = 3.2;
static const double NOVA_RADIUS = 5.6;
static const double NOVA_DAMAGE = 34.0;
static const double BODY_PUSH_RADIUS = 1.35;
static const double PLAYER_BODY_RADIUS = 0.3;
static const double BODY_PUSH_STIFFNESS = 16.0;

static const char *ACTION_NAMES[] = { "none", "swipe", "combo", "lunge", "bolts", "curse", "storm", "nova",
                                      "summon", "shield", "jump", "jump_air", "jump_land" };

void KoscheiController3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &KoscheiController3D::take_damage);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &KoscheiController3D::apply_knockback);
    ClassDB::bind_method(D_METHOD("set_arena_half", "half"), &KoscheiController3D::set_arena_half);
    ClassDB::bind_method(D_METHOD("get_health"), &KoscheiController3D::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &KoscheiController3D::get_max_health);
    ClassDB::bind_method(D_METHOD("get_hit_radius"), &KoscheiController3D::get_hit_radius);
    ClassDB::bind_method(D_METHOD("get_phase"), &KoscheiController3D::get_phase);
    ClassDB::bind_method(D_METHOD("is_invulnerable"), &KoscheiController3D::is_invulnerable);
    ClassDB::bind_method(D_METHOD("is_airborne"), &KoscheiController3D::is_airborne);
    ClassDB::bind_method(D_METHOD("get_shield_left"), &KoscheiController3D::get_shield_left);
    ClassDB::bind_method(D_METHOD("get_windup_progress"), &KoscheiController3D::get_windup_progress);
    ClassDB::bind_method(D_METHOD("get_facing_dir"), &KoscheiController3D::get_facing_dir);
    ClassDB::bind_method(D_METHOD("get_action_name"), &KoscheiController3D::get_action_name);

    ADD_SIGNAL(MethodInfo("defeated"));
    ADD_SIGNAL(MethodInfo("spawn_requested",
            PropertyInfo(Variant::STRING, "kind"),
            PropertyInfo(Variant::VECTOR3, "position"),
            PropertyInfo(Variant::DICTIONARY, "data")));
}

KoscheiController3D::KoscheiController3D() {
    for (int i = 0; i < ACTION_COUNT; ++i) cd[i] = 0.0;
    for (int i = 0; i < 4; ++i) history[i] = ACTION_NONE;
    // стартовые перезарядки: первые секунды боя — "знакомство", не сразу всё
    cd[ACTION_SHIELD] = 12.0;
    cd[ACTION_SUMMON] = 6.0;
    cd[ACTION_NOVA] = 7.0;
    cd[ACTION_CURSE] = 5.0;
}

void KoscheiController3D::_ready() {
    add_to_group(StringName("enemies"));
    add_to_group(StringName("bosses"));
    add_to_group(StringName("koschei"));

    visual = get_node_or_null(NodePath("Visual"));
    telegraph_visual = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("Telegraph")));
    melee_telegraph = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("MeleeTelegraph")));
    lunge_telegraph = Object::cast_to<MeshInstance3D>(get_node_or_null(NodePath("LungeTelegraph")));

    health = max_health;
    set_motion_mode(CharacterBody3D::MOTION_MODE_FLOATING);
    hide_telegraphs();
}

// ===================================================================== утилиты

Node3D *KoscheiController3D::get_player() const { return find_player_3d(this); }

int KoscheiController3D::count_group(const char *name) const {
    if (!is_inside_tree()) return 0;
    return int(get_tree()->get_nodes_in_group(StringName(name)).size());
}

void KoscheiController3D::request_spawn(const String &kind, const Vector3 &position, const Dictionary &data) {
    emit_signal(StringName("spawn_requested"), kind, position, data);
}

double KoscheiController3D::tempo() const { return phase == 3 ? 0.78 : (phase == 2 ? 0.9 : 1.0); }
double KoscheiController3D::damage_scale() const { return phase == 3 ? 1.35 : (phase == 2 ? 1.15 : 1.0); }
double KoscheiController3D::cd_scale() const { return phase == 3 ? 0.65 : (phase == 2 ? 0.82 : 1.0); }

Vector3 KoscheiController3D::clamp_to_arena(const Vector3 &p, double margin) const {
    Vector3 r = p;
    r.x = real_t(std::clamp(double(r.x), -(double(arena_half.x) - margin), double(arena_half.x) - margin));
    r.z = real_t(std::clamp(double(r.z), -(double(arena_half.y) - margin), double(arena_half.y) - margin));
    r.y = 0.0;
    return r;
}

Vector3 KoscheiController3D::point_near(const Vector3 &center, double min_r, double max_r, double margin) const {
    const double angle = UtilityFunctions::randf_range(0.0, 6.2831853);
    const double r = UtilityFunctions::randf_range(min_r, max_r);
    return clamp_to_arena(center + Vector3(std::cos(angle) * r, 0.0, std::sin(angle) * r), margin);
}

Vector3 KoscheiController3D::random_far_point(const Vector3 &from, double min_distance, double margin) const {
    Vector3 best = clamp_to_arena(Vector3(0.0, 0.0, 0.0), margin);
    double best_d = 0.0;
    for (int i = 0; i < 16; ++i) {
        const Vector3 c = clamp_to_arena(Vector3(UtilityFunctions::randf_range(-arena_half.x, arena_half.x),
                                                 0.0, UtilityFunctions::randf_range(-arena_half.y, arena_half.y)), margin);
        Vector3 d = c - from;
        d.y = 0.0;
        const double dist = d.length();
        if (dist >= min_distance) return c;
        if (dist > best_d) { best_d = dist; best = c; }
    }
    return best;
}

Vector3 KoscheiController3D::predicted_player(Node3D *player, double lead) const {
    Vector3 p = player->get_global_position() + Vector3(player_vel.x, 0.0, player_vel.z) * lead;
    return clamp_to_arena(p, 0.8);
}

void KoscheiController3D::hide_telegraphs() {
    if (telegraph_visual != nullptr) {
        telegraph_visual->set_visible(false);
        telegraph_visual->set_position(Vector3(0.0, 0.05, 0.0));
    }
    if (melee_telegraph != nullptr) melee_telegraph->set_visible(false);
    if (lunge_telegraph != nullptr) lunge_telegraph->set_visible(false);
}

void KoscheiController3D::command_sword(const String &name) {
    if (!is_inside_tree()) return;
    Node *sword = get_tree()->get_first_node_in_group(StringName("kladenets"));
    if (sword != nullptr && sword->has_method(StringName("command"))) sword->call(StringName("command"), name);
}

void KoscheiController3D::push_history(Action a) {
    for (int i = 3; i > 0; --i) history[i] = history[i - 1];
    history[0] = a;
}

bool KoscheiController3D::in_history(Action a, int depth) const {
    for (int i = 0; i < depth && i < 4; ++i) if (history[i] == a) return true;
    return false;
}

double KoscheiController3D::recent_damage(double window) const {
    double sum = 0.0;
    for (const auto &e : damage_log) if (time_now - e.first <= window) sum += e.second;
    return sum;
}

double KoscheiController3D::dash_rate() const {
    int n = 0;
    for (double t : dash_times) if (time_now - t <= 6.0) ++n;
    return double(n);
}

// ===================================================================== телеметрия героя

void KoscheiController3D::update_telemetry(Node3D *player, double delta) {
    const Vector3 pp = player->get_global_position();
    if (!telemetry_ready) {
        last_player_pos = pp;
        telemetry_ready = true;
    }
    if (delta > 0.0) {
        const Vector3 v = (pp - last_player_pos) / delta;
        player_vel = player_vel.lerp(v, 1.0 - std::exp(-9.0 * delta));
    }
    last_player_pos = pp;

    const double speed = Vector2(player_vel.x, player_vel.z).length();
    speed_avg += (speed - speed_avg) * (1.0 - std::exp(-delta / 2.0));

    Vector3 d = pp - get_global_position();
    d.y = 0.0;
    const double dist = d.length();
    avg_dist += (dist - avg_dist) * (1.0 - std::exp(-delta / 4.0));

    if (dist < 3.4) melee_time = std::min(6.0, melee_time + delta);
    else melee_time = std::max(0.0, melee_time - delta * 0.6);
    if (speed < 0.4) idle_time += delta; else idle_time = 0.0;

    if (player->has_method(StringName("get_health"))) {
        const double hp = double(player->call(StringName("get_health")));
        if (last_player_hp >= 0.0 && hp < last_player_hp - 0.01) since_hurt = 0.0; else since_hurt += delta;
        last_player_hp = hp;
    }
    if (player->has_method(StringName("is_dashing"))) {
        const bool dashing = bool(player->call(StringName("is_dashing")));
        if (dashing && !prev_player_dashing) dash_times.push_back(time_now);
        prev_player_dashing = dashing;
    }
    while (!dash_times.empty() && time_now - dash_times.front() > 6.0) dash_times.erase(dash_times.begin());
    while (!damage_log.empty() && time_now - damage_log.front().first > 6.0) damage_log.erase(damage_log.begin());
}

void KoscheiController3D::update_phase() {
    const double ratio = health / max_health;
    phase = ratio > 0.66 ? 1 : (ratio > 0.33 ? 2 : 3);
}

void KoscheiController3D::update_shield_triggers() {
    if (shield_left > 0.0 || force_shield || action == ACTION_SHIELD) return;
    const double ratio = health / max_health;
    if (!shield_66_used && ratio <= 0.66) { shield_66_used = true; force_shield = true; return; }
    if (!shield_33_used && ratio <= 0.33) { shield_33_used = true; force_shield = true; return; }
    // реакция на burst: если за 3 с по нему нанесли >14% HP — прячется под щит
    if (cd[ACTION_SHIELD] <= 0.0 && ratio < 0.92 && recent_damage(3.0) >= 0.14 * max_health) force_shield = true;
}

// ===================================================================== удары когтями

void KoscheiController3D::queue_hit(double delay, double damage, double reach, double cos_half, double step) {
    PendingHit h;
    h.delay = delay;
    h.damage = damage * damage_scale();
    h.reach = reach;
    h.cos_half = cos_half;
    h.step = step;
    hits.push_back(h);
}

void KoscheiController3D::update_pending_hits(Node3D *player, double delta) {
    if (hits.empty() || player == nullptr) return;
    for (size_t i = 0; i < hits.size();) {
        PendingHit &h = hits[i];
        h.delay -= delta;
        Vector3 to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
        // направление удара фиксируется за 0.12 с до попадания (видно, куда уворачиваться)
        if (!h.aimed && h.delay <= 0.12 && to_player.length_squared() > 0.0001) {
            h.dir = to_player.normalized();
            h.aimed = true;
            facing_dir = h.dir;
            if (h.step > 0.0) {
                dash_dir = h.dir;
                dash_speed = h.step / 0.12;
                dash_left = 0.12;
            }
        }
        if (h.delay <= 0.0) {
            const double dist = to_player.length();
            if (h.aimed && dist <= h.reach && dist > 0.001 &&
                    to_player.normalized().dot(h.dir) >= h.cos_half && player->has_method(StringName("take_damage"))) {
                player->call(StringName("take_damage"), h.damage);
            }
            hits.erase(hits.begin() + i);
        } else {
            ++i;
        }
    }
}

// ===================================================================== основной цикл

void KoscheiController3D::_physics_process(double delta) {
    time_now += delta;
    hit_flash_left = std::max(0.0, hit_flash_left - delta);
    shield_left = std::max(0.0, shield_left - delta);
    for (int i = 0; i < ACTION_COUNT; ++i) cd[i] = std::max(0.0, cd[i] - delta);
    update_phase();

    Node3D *player = get_player();
    Vector3 to_player;
    if (player != nullptr) {
        update_telemetry(player, delta);
        to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
    }
    update_shield_triggers();
    update_pending_hits(player, delta);
    update_visuals(to_player);
    if (player != nullptr && !airborne) push_player_out(player, to_player, delta);

    // --- рывок (выпад / шаг при ударе) ---
    if (dash_left > 0.0) {
        dash_left -= delta;
        set_velocity(dash_dir * dash_speed);
        move_and_slide();
        set_global_position(clamp_to_arena(get_global_position(), 0.6));
        if (lunge_active && player != nullptr && !lunge_hit_done && to_player.length() <= 2.1) {
            lunge_hit_done = true;
            if (player->has_method(StringName("take_damage"))) player->call(StringName("take_damage"), LUNGE_DAMAGE * damage_scale());
        }
        if (dash_left <= 0.0) lunge_active = false;
        if (action == ACTION_NONE) return;   // во время рывка — без решений
    }

    // --- реакция: щит включается сразу, даже посреди восстановления ---
    if (force_shield && action == ACTION_NONE && !airborne && player != nullptr) {
        force_shield = false;
        recover_left = 0.0;
        begin_action(ACTION_SHIELD, player);
    }

    // --- замах / полёт ---
    if (action != ACTION_NONE) {
        windup_left -= delta;
        if (action == ACTION_JUMP_AIR) {
            const double t = std::clamp(1.0 - windup_left / std::max(0.01, windup_total), 0.0, 1.0);
            Vector3 p = jump_from.lerp(jump_to, t);
            p.y = 3.6 * 4.0 * t * (1.0 - t);
            set_global_position(p);
            Vector3 flight = jump_to - jump_from;
            flight.y = 0.0;
            if (flight.length_squared() > 0.01) facing_dir = flight.normalized();
        } else if (action != ACTION_JUMP_LAND) {
            set_velocity(Vector3());
            move_and_slide();
        }
        if (windup_left <= 0.0) execute_action(player);
        return;
    }

    if (recover_left > 0.0) {
        recover_left -= delta;
        set_velocity(Vector3());
        move_and_slide();
        return;
    }

    // --- движение: преследует героя по полосе дистанции и "кружит" вокруг ---
    Vector3 desired;
    if (player != nullptr) {
        const double dist = to_player.length();
        const Vector3 dir = dist > 0.001 ? to_player / dist : Vector3(0.0, 0.0, 1.0);
        const Vector3 side = Vector3(-dir.z, 0.0, dir.x);
        const double speed = (phase == 3 ? 3.5 : (phase == 2 ? 3.0 : 2.5));
        const bool stunned = player->has_method(StringName("get_stun_left")) && double(player->call(StringName("get_stun_left"))) > 0.0;

        strafe_left -= delta;
        if (strafe_left <= 0.0) {
            strafe_dir = (UtilityFunctions::randf() < 0.5) ? -1.0 : 1.0;
            strafe_left = UtilityFunctions::randf_range(1.6, 3.4);
        }
        const bool wants_ranged = time_now < prefer_ranged_until;
        if (stunned || time_now < punish_until) {
            if (dist > 2.4) desired = dir * speed * 1.6;                       // добивать
        } else if (wants_ranged) {
            if (dist < 6.0) desired = (-dir * 0.7 + side * strafe_dir * 0.6) * speed;   // держит дистанцию для заклинаний
        } else if (dist > 5.6) {
            desired = dir * speed;
        } else if (dist > 3.0) {
            desired = side * strafe_dir * speed * 0.75 + dir * speed * 0.25;
        }
    }
    set_velocity(desired);
    move_and_slide();
    set_global_position(clamp_to_arena(get_global_position(), 0.6));

    decision_left -= delta;
    if (decision_left <= 0.0 && player != nullptr) choose_action(player);
}

// ===================================================================== утилитарный выбор способности

void KoscheiController3D::choose_action(Node3D *player) {
    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    const double dist = to_player.length();

    const bool stunned = player->has_method(StringName("get_stun_left")) && double(player->call(StringName("get_stun_left"))) > 0.0;
    double hp_frac = 1.0;
    if (player->has_method(StringName("get_health")) && player->has_method(StringName("get_max_health"))) {
        const double mx = double(player->call(StringName("get_max_health")));
        if (mx > 0.0) hp_frac = double(player->call(StringName("get_health"))) / mx;
    }

    // "Читает" героя: кайтит издалека, прижимается вплотную, стоит на месте, спамит дэш, быстро ли его бьют.
    const bool kiting = avg_dist > 7.2;
    const bool hugging = melee_time > 2.4;
    const bool stationary = idle_time > 0.9;
    const bool dash_spam = dash_rate() >= 3.0;
    const bool burst = recent_damage(4.0) > 0.12 * max_health;
    const bool punish = stunned || time_now < punish_until;
    const bool ranged_pref = time_now < prefer_ranged_until;
    const int skeletons = count_group("minions");
    const int skeleton_cap = phase == 1 ? 4 : (phase == 2 ? 5 : 6);

    double s[ACTION_COUNT];
    for (int i = 0; i < ACTION_COUNT; ++i) s[i] = 0.0;

    // --- ближний бой (когти) ---
    if (dist <= SWIPE_REACH - 0.2) {
        s[ACTION_SWIPE] = 6.0;
        s[ACTION_COMBO] = 5.5;
        if (stunned) { s[ACTION_COMBO] += 8.0; s[ACTION_SWIPE] += 3.0; }
        if (punish) s[ACTION_COMBO] += 4.0;
        if (hp_frac < 0.4) s[ACTION_COMBO] += 2.0;
        if (ranged_pref) { s[ACTION_SWIPE] *= 0.5; s[ACTION_COMBO] *= 0.5; }
    }
    // --- средняя дистанция: выпад-рывок (с упреждением) ---
    if (dist > 3.4 && dist <= LUNGE_MAX + 1.5) {
        s[ACTION_LUNGE] = 5.0;
        if (kiting) s[ACTION_LUNGE] += 3.0;
        if (punish) s[ACTION_LUNGE] += 3.0;
        if (dash_spam) s[ACTION_LUNGE] -= 2.0;      // дэшер уходит от прямой линии — ему лучше зонная магия
        if (ranged_pref) s[ACTION_LUNGE] *= 0.5;
    }
    // --- заклинания ---
    if (dist >= 3.5) {
        s[ACTION_BOLTS] = 3.0 + (kiting ? 4.0 : 0.0) + (ranged_pref ? 3.0 : 0.0) + (dash_spam ? 1.0 : 0.0);
        s[ACTION_CURSE] = 3.0 + (burst ? 3.0 : 0.0) + (hp_frac < 0.5 ? 2.0 : 0.0);
        if (stunned) s[ACTION_CURSE] = 0.0;
    }
    s[ACTION_STORM] = 4.0 + (stationary ? 3.5 : 0.0) + (dash_spam ? 2.0 : 0.0) + (hugging ? 1.5 : 0.0) + (ranged_pref ? 2.0 : 0.0);
    if (dist <= 5.0) s[ACTION_NOVA] = 3.0 + (hugging ? 8.0 : 0.0) + (melee_time > 1.5 ? 2.0 : 0.0);
    if (skeletons < skeleton_cap - 1) {
        s[ACTION_SUMMON] = 3.5 + (kiting ? 2.0 : 0.0) + (phase >= 2 ? 1.0 : 0.0) + (skeletons == 0 ? 2.0 : 0.0);
    }
    s[ACTION_JUMP] = 3.0 + (kiting ? 6.0 : 0.0) + (hugging ? 5.0 : 0.0) + (dist > 9.0 ? 3.0 : 0.0);

    // --- готовность, штрафы за повторы, щадящее окно после попадания, шум ---
    double total = 0.0;
    for (int i = 1; i < ACTION_JUMP_AIR; ++i) {
        const Action a = static_cast<Action>(i);
        if (a == ACTION_SHIELD) { s[i] = 0.0; continue; }       // щит включается реакциями, а не выбором
        if (cd[i] > 0.0) { s[i] = 0.0; continue; }
        if (a == history[0]) s[i] *= 0.15;
        else if (in_history(a, 3)) s[i] *= 0.6;
        if (since_hurt < 0.5 && (a == ACTION_SWIPE || a == ACTION_COMBO || a == ACTION_LUNGE)) s[i] *= 0.35;
        s[i] *= UtilityFunctions::randf_range(0.85, 1.25);
        s[i] = std::max(0.0, s[i]);
        s[i] *= s[i];                                            // чёткий приоритет лучших вариантов, но не детерминированный
        total += s[i];
    }
    if (total <= 0.0) {
        decision_left = 0.25;
        return;
    }
    double pick = UtilityFunctions::randf() * total;
    for (int i = 1; i < ACTION_JUMP_AIR; ++i) {
        if (s[i] <= 0.0) continue;
        pick -= s[i];
        if (pick <= 0.0) {
            begin_action(static_cast<Action>(i), player);
            return;
        }
    }
    begin_action(ACTION_SWIPE, player);
}

void KoscheiController3D::begin_action(Action a, Node3D *player) {
    action = a;
    Vector3 to_player = player->get_global_position() - get_global_position();
    to_player.y = 0.0;
    if (to_player.length_squared() > 0.0001) facing_dir = to_player.normalized();

    const double t = tempo();
    switch (a) {
        case ACTION_SWIPE:
            windup_total = 0.45 * t;
            if (melee_telegraph != nullptr) melee_telegraph->set_visible(true);
            break;
        case ACTION_COMBO:
            windup_total = 0.40 * t;
            if (melee_telegraph != nullptr) melee_telegraph->set_visible(true);
            break;
        case ACTION_LUNGE: {
            windup_total = 0.55 * t;
            lunge_target = predicted_player(player, 0.45);
            Vector3 d = lunge_target - get_global_position();
            d.y = 0.0;
            lunge_length = std::clamp(d.length() + 1.0, 4.0, LUNGE_MAX);
            dash_dir = d.length_squared() > 0.0001 ? d.normalized() : facing_dir;
            facing_dir = dash_dir;
            if (lunge_telegraph != nullptr) {
                lunge_telegraph->set_rotation(Vector3(0.0, std::atan2(-dash_dir.z, dash_dir.x), 0.0));
                lunge_telegraph->set_scale(Vector3(lunge_length, 1.0, 1.0));
                lunge_telegraph->set_visible(true);
            }
        } break;
        case ACTION_BOLTS: windup_total = 0.5 * t; break;
        case ACTION_CURSE: windup_total = 0.7 * t; break;
        case ACTION_STORM: {
            windup_total = std::max(0.55, 0.8 * t);
            const double r = UtilityFunctions::randf();
            if (phase >= 2 && r < 0.25) storm_variant = 3;                     // ядовитое поле
            else if (melee_time > 2.4) storm_variant = 2;                      // прижали — кольцо вокруг героя
            else if (avg_dist > 7.2) storm_variant = (r < 0.5) ? 1 : 0;        // кайтит — дорожка / упреждение
            else if (idle_time > 0.9) storm_variant = 2;                       // стоит — кольцо
            else storm_variant = int(UtilityFunctions::randi_range(0, 2));
        } break;
        case ACTION_NOVA:
            windup_total = 0.9 * t;
            if (telegraph_visual != nullptr) {
                telegraph_visual->set_position(Vector3(0.0, 0.05, 0.0));
                telegraph_visual->set_scale(Vector3(NOVA_RADIUS, 1.0, NOVA_RADIUS));
                telegraph_visual->set_visible(true);
            }
            break;
        case ACTION_SUMMON: windup_total = 1.0 * t; break;
        case ACTION_SHIELD: windup_total = 0.6; break;
        case ACTION_JUMP: {
            windup_total = 0.45 * t;
            const Vector3 pp = player->get_global_position();
            if (avg_dist > 7.2 || to_player.length() > 9.0) {
                jump_to = predicted_player(player, 0.9);     // догоняет — прыжок-удар в упреждённую точку
                jump_slam = true;
            } else if (melee_time > 2.4 || to_player.length() < 3.0) {
                jump_to = random_far_point(pp, 6.5, 1.4);    // прижали — отпрыгивает и колдует
                jump_slam = false;
            } else {
                jump_to = point_near(pp, 2.5, 4.0, 1.4);
                jump_slam = true;
            }
            jump_to = clamp_to_arena(jump_to, 1.3);
            jump_from = get_global_position();
            if (telegraph_visual != nullptr) {
                const double r = jump_slam ? SLAM_RADIUS : 1.2;
                telegraph_visual->set_global_position(jump_to + Vector3(0.0, 0.05, 0.0));
                telegraph_visual->set_scale(Vector3(r, 1.0, r));
                telegraph_visual->set_visible(true);
            }
        } break;
        case ACTION_JUMP_LAND: windup_total = 0.45; break;
        default: windup_total = 0.5; break;
    }
    windup_left = windup_total;
}

// ===================================================================== применение

Dictionary KoscheiController3D::choose_curse(Node3D *player) {
    double hp_frac = 1.0;
    if (player->has_method(StringName("get_health")) && player->has_method(StringName("get_max_health"))) {
        const double mx = double(player->call(StringName("get_max_health")));
        if (mx > 0.0) hp_frac = double(player->call(StringName("get_health"))) / mx;
    }
    Dictionary d;
    int kind;
    if (hp_frac < 0.5) kind = 0;                       // слабого героя делает уязвимым
    else if (recent_damage(4.0) > 0.12 * max_health) kind = 1;   // сильно бьёт — ослабляет урон
    else if (avg_dist > 7.2) kind = 2;                 // убегает — замедляет
    else kind = int(UtilityFunctions::randi_range(1, 3));
    switch (kind) {
        case 0: d["vuln_mult"] = 1.35; d["vuln_time"] = 8.0; break;
        case 1: d["weaken_mult"] = 0.65; d["weaken_time"] = 8.0; break;
        case 2: d["slow_factor"] = 0.55; d["slow_time"] = 5.0; break;
        default: d["silence_time"] = 6.0; break;
    }
    return d;
}

void KoscheiController3D::fire_bolt(const Vector3 &dir, double speed, double damage, double radius, double homing, const Dictionary &debuff) {
    Dictionary d = debuff.duplicate();
    const Vector3 from = get_global_position() + Vector3(0.0, 1.7, 0.0);
    d["kind"] = String("bolt");
    d["arc"] = false;
    d["from"] = from;
    d["direction"] = dir;
    d["speed"] = speed;
    d["max_distance"] = 30.0;
    d["damage"] = damage * damage_scale();
    d["radius"] = radius;
    d["homing"] = homing;
    request_spawn("projectile", from, d);
}

void KoscheiController3D::spawn_blast(const Vector3 &pos, double radius, double telegraph, double damage) {
    Dictionary d;
    d["kind"] = String("blast");
    d["radius"] = radius;
    d["telegraph"] = telegraph;
    d["active"] = 0.45;
    d["damage"] = damage;
    d["explodes"] = true;
    request_spawn("blast", pos, d);
}

void KoscheiController3D::execute_action(Node3D *player) {
    const Vector3 self_pos = get_global_position();
    const Action finished = action;
    double recover = 0.45;
    hide_telegraphs();

    Vector3 to_player;
    if (player != nullptr) {
        to_player = player->get_global_position() - self_pos;
        to_player.y = 0.0;
    }

    switch (finished) {
        case ACTION_SWIPE: {
            queue_hit(0.10, SWIPE_DAMAGE, SWIPE_REACH, SWIPE_COS, 1.2);
            recover = 0.40;
        } break;

        case ACTION_COMBO: {
            queue_hit(0.10, 18.0, 3.7, 0.38, 1.0);
            queue_hit(0.40, 18.0, 3.7, 0.38, 1.0);
            queue_hit(0.70, 32.0, 4.2, 0.35, 1.6);
            cd[ACTION_COMBO] = 5.0 * cd_scale();
            recover = 1.05;
        } break;

        case ACTION_LUNGE: {
            dash_speed = LUNGE_SPEED;
            dash_left = lunge_length / LUNGE_SPEED;
            lunge_active = true;
            lunge_hit_done = false;
            cd[ACTION_LUNGE] = 6.0 * cd_scale();
            recover = 0.55;                 // после рывка — окно для контратаки (таймер стоит, пока длится рывок)
        } break;

        case ACTION_BOLTS: {
            const int n = (phase == 1) ? 3 : (phase == 2 ? 5 : 7);
            Vector3 dir = (player != nullptr) ? (predicted_player(player, 0.3) - self_pos) : facing_dir;
            dir.y = 0.0;
            dir = dir.length_squared() > 0.0001 ? dir.normalized() : facing_dir;
            for (int i = 0; i < n; ++i) {
                const double ang = (double(i) - double(n - 1) * 0.5) * 0.23;
                fire_bolt(dir.rotated(Vector3(0.0, 1.0, 0.0), ang), 10.0, 12.0, 0.35, 1.1, Dictionary());
            }
            cd[ACTION_BOLTS] = 5.0 * cd_scale();
            recover = 0.5;
        } break;

        case ACTION_CURSE: {
            Vector3 dir = to_player.length_squared() > 0.0001 ? to_player.normalized() : facing_dir;
            fire_bolt(dir, 7.5, 6.0, 0.5, 2.4, choose_curse(player));
            if (phase == 3) fire_bolt(dir.rotated(Vector3(0.0, 1.0, 0.0), 0.5), 7.5, 6.0, 0.5, 2.4, choose_curse(player));
            cd[ACTION_CURSE] = 9.0 * cd_scale();
            punish_until = time_now + 4.0;       // связка: проклял -> бежит добивать когтями
            recover = 0.5;
        } break;

        case ACTION_STORM: {
            const Vector3 pp = (player != nullptr) ? player->get_global_position() : self_pos;
            const double dmg = 26.0 * damage_scale();
            const double rad = phase == 3 ? 2.35 : 2.1;
            if (storm_variant == 0) {                    // с упреждением по скорости героя
                const int n = phase == 3 ? 5 : 3;
                for (int i = 0; i < n; ++i) {
                    const Vector3 p = (i == 0) ? pp : clamp_to_arena(pp + Vector3(player_vel.x, 0.0, player_vel.z) * (0.45 * i), 0.8);
                    spawn_blast(p, rad, 1.0 + 0.18 * i, dmg);
                }
            } else if (storm_variant == 1) {             // дорожка от босса к герою (каскад)
                Vector3 dir = to_player.length_squared() > 0.0001 ? to_player.normalized() : facing_dir;
                for (int i = 0; i < 6; ++i) spawn_blast(clamp_to_arena(self_pos + dir * (2.6 + 2.3 * i), 0.8), rad, 1.0 + 0.16 * i, dmg);
            } else if (storm_variant == 2) {             // кольцо вокруг героя + центр с задержкой
                const double base = UtilityFunctions::randf_range(0.0, 6.2831853);
                for (int i = 0; i < 6; ++i) {
                    const double a = base + i * 1.0471976;
                    spawn_blast(clamp_to_arena(pp + Vector3(std::cos(a) * 3.1, 0.0, std::sin(a) * 3.1), 0.8), rad, 1.0, dmg);
                }
                spawn_blast(pp, rad + 0.2, 1.7, dmg);
            } else {                                     // ядовитое поле, мешающее стоять на месте
                const int n = phase == 3 ? 3 : 2;
                for (int i = 0; i < n; ++i) {
                    Dictionary d;
                    d["kind"] = String("miasma");
                    d["radius"] = 2.7;
                    d["telegraph"] = 0.8;
                    d["active"] = 7.0;
                    d["damage"] = 6.0 * damage_scale();
                    d["tick"] = 0.5;
                    d["slow_factor"] = 0.7;
                    d["slow_time"] = 0.8;
                    request_spawn("miasma", (i == 0) ? pp : point_near(pp, 3.0, 5.5, 1.2), d);
                }
            }
            cd[ACTION_STORM] = 7.0 * cd_scale();
            recover = 0.55;
        } break;

        case ACTION_NOVA: {
            spawn_blast(Vector3(self_pos.x, 0.0, self_pos.z), NOVA_RADIUS, 0.02, NOVA_DAMAGE * damage_scale());
            if (player != nullptr && to_player.length() <= NOVA_RADIUS && player->has_method(StringName("apply_knockback"))) {
                const Vector3 away = to_player.length_squared() > 0.0001 ? to_player.normalized() : Vector3(0.0, 0.0, 1.0);
                player->call(StringName("apply_knockback"), away * 13.0);
            }
            command_sword("spin");               // меч подхватывает: вращается вокруг хозяина
            cd[ACTION_NOVA] = 11.0 * cd_scale();
            recover = 0.7;
        } break;

        case ACTION_SUMMON: {
            const int skeletons = count_group("minions");
            const int cap = phase == 1 ? 4 : (phase == 2 ? 5 : 6);
            int count = phase == 1 ? 2 : (phase == 2 ? 3 : 4);
            count = std::min(count, cap - skeletons);
            const Vector3 pp = (player != nullptr) ? player->get_global_position() : self_pos;
            for (int i = 0; i < count; ++i) {
                // в третьей фазе часть скелетов выходит из земли вокруг героя
                const Vector3 pos = (phase == 3 && (i % 2 == 1)) ? point_near(pp, 3.0, 3.8, 1.0) : point_near(self_pos, 2.2, 2.9, 1.0);
                request_spawn("skeleton", pos, Dictionary());
            }
            cd[ACTION_SUMMON] = 14.0 * cd_scale();
            prefer_ranged_until = time_now + 4.0;   // пока скелеты давят — зонирует заклинаниями
            recover = 0.6;
        } break;

        case ACTION_SHIELD: {
            shield_left = phase == 1 ? 4.0 : (phase == 2 ? 4.5 : 5.0);
            cd[ACTION_SHIELD] = 20.0;
            force_shield = false;
            command_sword((to_player.length() < 6.5) ? "spin" : "thrust");   // под щитом давит клинком
            recover = 0.35;
        } break;

        case ACTION_JUMP: {
            airborne = true;
            action = ACTION_JUMP_AIR;
            windup_total = 0.8;
            windup_left = 0.8;
            cd[ACTION_JUMP] = 8.0 * cd_scale();
            return;
        }

        case ACTION_JUMP_AIR: {
            airborne = false;
            set_global_position(Vector3(jump_to.x, 0.0, jump_to.z));
            if (jump_slam) {
                spawn_blast(jump_to, SLAM_RADIUS, 0.02, SLAM_DAMAGE * damage_scale());
                punish_until = time_now + 3.0;          // приземлился рядом — сразу комбо
            } else {
                prefer_ranged_until = time_now + 5.0;   // отпрыгнул — теперь заклинания
            }
            action = ACTION_JUMP_LAND;
            windup_total = 0.45;
            windup_left = 0.45;
            return;
        }

        case ACTION_JUMP_LAND: {
            recover = 0.12;
        } break;

        default: break;
    }

    push_history(finished);
    action = ACTION_NONE;
    recover_left = recover;
    decision_left = (phase == 1) ? 0.9 : (phase == 2 ? 0.7 : 0.5);
}

// ===================================================================== визуал и прочее

void KoscheiController3D::update_visuals(const Vector3 &to_player) {
    if (action != ACTION_JUMP_AIR && dash_left <= 0.0 && action != ACTION_LUNGE && to_player.length_squared() > 0.01) {
        facing_dir = to_player.normalized();
    }
    // удар когтями: телеграф-прямоугольник следит за героем до конца замаха
    if (melee_telegraph != nullptr && melee_telegraph->is_visible() && to_player.length_squared() > 0.01) {
        melee_telegraph->set_rotation(Vector3(0.0, std::atan2(-to_player.z, to_player.x), 0.0));
    }

    Color tint = Color(1.0, 1.0, 1.0, 1.0);
    if (phase == 2) tint = Color(0.92, 0.82, 1.0, 1.0);
    else if (phase == 3) tint = Color(1.0, 0.66, 0.84, 1.0);
    if (action != ACTION_NONE && action != ACTION_JUMP_AIR && action != ACTION_JUMP_LAND) {
        const double pulse = 0.5 + 0.5 * std::sin(windup_left * 26.0);
        if (action == ACTION_SWIPE || action == ACTION_COMBO || action == ACTION_LUNGE) tint = Color(1.0 + 0.3 * pulse, 0.78, 0.78, 1.0);
        else tint = Color(0.82, 0.78 + 0.1 * pulse, 1.0 + 0.3 * pulse, 1.0);
    }
    if (shield_left > 0.0) {
        const double pulse = 0.5 + 0.5 * std::sin(time_now * 9.0);
        tint = Color(1.0, 0.92 + 0.08 * pulse, 0.55 + 0.25 * pulse, 1.0);
    } else if (hit_flash_left > 0.0) {
        tint = Color(tint.r * 1.5, tint.g * 1.5, tint.b * 1.5, 1.0);
    }
    if (visual != nullptr && visual->has_method(StringName("set_tint"))) visual->call(StringName("set_tint"), tint);
}

void KoscheiController3D::push_player_out(Node3D *player, const Vector3 &to_player, double delta) {
    // Тело босса твёрдое для героя; вытесняется только герой (босса герой не двигает).
    if (!player->has_method(StringName("push_out"))) return;
    const double min_dist = BODY_PUSH_RADIUS + PLAYER_BODY_RADIUS;
    const double dist = to_player.length();
    if (dist >= min_dist) return;
    const Vector3 dir = (dist > 0.001) ? (to_player / dist) : Vector3(0.0, 0.0, 1.0);
    player->call(StringName("push_out"), dir * ((min_dist - dist) * std::min(1.0, delta * BODY_PUSH_STIFFNESS)));
}

void KoscheiController3D::take_damage(double amount) {
    if (amount <= 0.0 || health <= 0.0) return;
    if (shield_left > 0.0) return;          // временная неуязвимость
    health = std::max(0.0, health - amount);
    hit_flash_left = 0.07;
    damage_log.push_back({ time_now, amount });
    if (health <= 0.0) {
        command_sword("die");
        emit_signal(StringName("defeated"));
        queue_free();
    }
}

void KoscheiController3D::apply_knockback(const Vector3 &) {}
void KoscheiController3D::set_arena_half(const Vector2 &half) { arena_half = half; }

double KoscheiController3D::get_health() const { return health; }
double KoscheiController3D::get_max_health() const { return max_health; }
double KoscheiController3D::get_hit_radius() const { return 1.35; }
int KoscheiController3D::get_phase() const { return phase; }
bool KoscheiController3D::is_invulnerable() const { return shield_left > 0.0; }
bool KoscheiController3D::is_airborne() const { return airborne; }
double KoscheiController3D::get_shield_left() const { return shield_left; }
Vector3 KoscheiController3D::get_facing_dir() const { return facing_dir; }

double KoscheiController3D::get_windup_progress() const {
    if (action == ACTION_NONE || windup_total <= 0.0) return 0.0;
    return std::clamp(1.0 - windup_left / windup_total, 0.0, 1.0);
}

String KoscheiController3D::get_action_name() const { return ACTION_NAMES[action]; }

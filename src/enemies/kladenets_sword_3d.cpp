#include <algorithm>
#include <cmath>

#include "kladenets_sword_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

static const double DIVE_DAMAGE = 30.0;
static const double DIVE_RADIUS = 1.6;
static const double THRUST_DAMAGE = 26.0;
static const double THRUST_WIDTH = 1.0;
static const double THRUST_LENGTH = 11.0;
static const double SPIN_DAMAGE = 14.0;
static const double SPIN_RADIUS = 4.4;

void KladenetsSword3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("command", "name"), &KladenetsSword3D::command);
    ClassDB::bind_method(D_METHOD("get_mode_name"), &KladenetsSword3D::get_mode_name);
    ClassDB::bind_method(D_METHOD("get_marker_kind"), &KladenetsSword3D::get_marker_kind);
    ClassDB::bind_method(D_METHOD("get_marker_position"), &KladenetsSword3D::get_marker_position);
    ClassDB::bind_method(D_METHOD("get_marker_direction"), &KladenetsSword3D::get_marker_direction);
    ClassDB::bind_method(D_METHOD("get_marker_length"), &KladenetsSword3D::get_marker_length);
    ClassDB::bind_method(D_METHOD("get_marker_radius"), &KladenetsSword3D::get_marker_radius);
}

void KladenetsSword3D::_ready() {
    add_to_group(StringName("kladenets"));
    orbit_angle = UtilityFunctions::randf_range(0.0, 6.2831853);
    attack_cd = 2.5;
}

Node3D *KladenetsSword3D::get_boss() const {
    if (!is_inside_tree()) return nullptr;
    return Object::cast_to<Node3D>(get_tree()->get_first_node_in_group(StringName("koschei")));
}

double KladenetsSword3D::boss_phase() const {
    Node3D *boss = get_boss();
    if (boss == nullptr || !boss->has_method(StringName("get_phase"))) return 1.0;
    return double(boss->call(StringName("get_phase")));
}

double KladenetsSword3D::damage_scale() const {
    const double p = boss_phase();
    return p >= 3.0 ? 1.35 : (p >= 2.0 ? 1.15 : 1.0);
}

bool KladenetsSword3D::boss_shielded() const {
    Node3D *boss = get_boss();
    return boss != nullptr && boss->has_method(StringName("is_invulnerable")) && bool(boss->call(StringName("is_invulnerable")));
}

void KladenetsSword3D::set_mode(Mode m) {
    mode = m;
    mode_time = 0.0;
    hit_done = false;
    marker_kind = "none";
}

void KladenetsSword3D::hurt_player(Node3D *player, double amount) {
    if (player != nullptr && player->has_method(StringName("take_damage"))) {
        player->call(StringName("take_damage"), amount * damage_scale());
    }
}

// Плавный поворот остриём в сторону dir. Базис строится вручную: z = остриё.
void KladenetsSword3D::apply_aim(const Vector3 &dir, double delta, double sharpness) {
    Vector3 d = dir;
    if (d.length_squared() < 1e-6) return;
    d = d.normalized();
    aim_dir = aim_dir.lerp(d, 1.0 - std::exp(-sharpness * delta));
    if (aim_dir.length_squared() < 1e-6) aim_dir = d;
    const Vector3 z = aim_dir.normalized();
    Vector3 up = Vector3(0.0, 1.0, 0.0);
    if (std::fabs(z.dot(up)) > 0.98) up = Vector3(1.0, 0.0, 0.0);
    const Vector3 x = up.cross(z).normalized();
    const Vector3 y = z.cross(x);
    set_basis(Basis(x, y, z));
}

Vector3 KladenetsSword3D::orbit_position(double radius, double height, double speed, double delta) {
    Node3D *boss = get_boss();
    const Vector3 center = (boss != nullptr) ? boss->get_global_position() : Vector3();
    orbit_angle += speed * delta;
    return center + Vector3(std::cos(orbit_angle) * radius, height, std::sin(orbit_angle) * radius);
}

void KladenetsSword3D::command(const String &name) {
    if (mode == MODE_DEAD) return;
    Node3D *player = find_player_3d(this);
    if (name == String("die")) {
        set_mode(MODE_DEAD);
    } else if (name == String("dive") && player != nullptr) {
        set_mode(MODE_DIVE_RISE);
        target_pos = player->get_global_position();
    } else if (name == String("thrust") && player != nullptr) {
        set_mode(MODE_THRUST_AIM);
    } else if (name == String("spin")) {
        set_mode(MODE_SPIN);
    } else if (name == String("orbit")) {
        set_mode(MODE_RETURN);
    }
}

void KladenetsSword3D::choose_attack(Node3D *player, Node3D *boss) {
    Vector3 to_player = player->get_global_position() - boss->get_global_position();
    to_player.y = 0.0;
    const double dist = to_player.length();

    // Меч выбирает атаку по обстановке: прижали босса — вращается, герой далеко — выпад, иначе удар сверху.
    double w_dive = 4.0;
    double w_thrust = (dist > 2.5) ? 4.0 : 1.0;
    double w_spin = (dist < 5.0) ? 5.0 : 1.2;
    if (boss_phase() < 2.0) w_spin *= 0.6;
    const double total = w_dive + w_thrust + w_spin;
    double pick = UtilityFunctions::randf() * total;

    if ((pick -= w_dive) <= 0.0) {
        set_mode(MODE_DIVE_RISE);
        target_pos = player->get_global_position();
    } else if ((pick -= w_thrust) <= 0.0) {
        set_mode(MODE_THRUST_AIM);
    } else {
        set_mode(MODE_SPIN);
    }
}

void KladenetsSword3D::_physics_process(double delta) {
    time_alive += delta;
    mode_time += delta;
    tick_cd = std::max(0.0, tick_cd - delta);

    Node3D *boss = get_boss();
    Node3D *player = find_player_3d(this);
    const double phase = boss_phase();
    const bool shielded = boss_shielded();

    if (mode == MODE_DEAD) {
        // Хозяин пал: меч падает на землю и замирает.
        Vector3 p = get_global_position();
        p.y = std::max(0.25, p.y - 9.0 * delta);
        set_global_position(p);
        apply_aim(Vector3(0.0, -0.4, 1.0), delta, 4.0);
        return;
    }
    if (boss == nullptr || player == nullptr) return;

    const Vector3 pp = player->get_global_position();

    switch (mode) {
        case MODE_ORBIT: {
            const double speed = (phase >= 3.0 ? 2.7 : (phase >= 2.0 ? 2.1 : 1.6)) * (shielded ? 1.5 : 1.0);
            const double radius = (shielded ? 2.3 : 3.1) + 0.4 * std::sin(time_alive * 1.7);
            const Vector3 p = orbit_position(radius, 1.7 + 0.25 * std::sin(time_alive * 2.3), speed, delta);
            set_global_position(get_global_position().lerp(p, 1.0 - std::exp(-9.0 * delta)));
            apply_aim(pp + Vector3(0.0, 1.0, 0.0) - get_global_position(), delta, 6.0);

            attack_cd -= delta * (shielded ? 1.8 : 1.0);
            if (attack_cd <= 0.0) {
                choose_attack(player, boss);
                const double base = phase >= 3.0 ? 2.3 : (phase >= 2.0 ? 3.1 : 4.0);
                attack_cd = base + UtilityFunctions::randf_range(0.0, 1.2);
            }
        } break;

        case MODE_DIVE_RISE: {
            // Поднимается над целью; первые 0.55 с следит за героем, затем фиксирует точку удара.
            if (mode_time < 0.55) target_pos = target_pos.lerp(pp, 1.0 - std::exp(-5.0 * delta));
            const Vector3 above = Vector3(target_pos.x, 4.2, target_pos.z);
            set_global_position(get_global_position().lerp(above, 1.0 - std::exp(-7.0 * delta)));
            apply_aim(Vector3(0.0, -1.0, 0.02), delta, 10.0);
            marker_kind = "circle";
            marker_pos = Vector3(target_pos.x, 0.0, target_pos.z);
            marker_radius = DIVE_RADIUS;
            if (mode_time >= 0.95) set_mode(MODE_DIVE_PLUNGE);
        } break;

        case MODE_DIVE_PLUNGE: {
            Vector3 p = get_global_position();
            p.x = target_pos.x;
            p.z = target_pos.z;
            p.y -= 34.0 * delta;
            marker_kind = "circle";
            marker_pos = Vector3(target_pos.x, 0.0, target_pos.z);
            marker_radius = DIVE_RADIUS;
            apply_aim(Vector3(0.0, -1.0, 0.02), delta, 20.0);
            if (p.y <= 0.35) {
                p.y = 0.35;
                Vector3 off = pp - Vector3(target_pos.x, 0.0, target_pos.z);
                off.y = 0.0;
                if (off.length() <= DIVE_RADIUS + 0.3) hurt_player(player, DIVE_DAMAGE);
                set_global_position(p);
                set_mode(MODE_DIVE_STUCK);
                return;
            }
            set_global_position(p);
        } break;

        case MODE_DIVE_STUCK: {
            apply_aim(Vector3(0.0, -1.0, 0.02), delta, 20.0);
            if (mode_time >= 0.7) set_mode(MODE_RETURN);
        } break;

        case MODE_THRUST_AIM: {
            // Отходит на дистанцию и целится; направление фиксируется за 0.22 с до рывка.
            Vector3 from_boss = pp - boss->get_global_position();
            from_boss.y = 0.0;
            const Vector3 dir = from_boss.length_squared() > 0.01 ? from_boss.normalized() : Vector3(0.0, 0.0, 1.0);
            const Vector3 hover = pp - dir * 4.6 + Vector3(0.0, 1.3, 0.0);
            set_global_position(get_global_position().lerp(hover, 1.0 - std::exp(-6.0 * delta)));
            Vector3 aim = pp - get_global_position();
            aim.y = 0.0;
            if (mode_time < 0.6 && aim.length_squared() > 0.01) dash_dir = aim.normalized();
            apply_aim(dash_dir, delta, 12.0);
            dash_start = get_global_position();
            marker_kind = "line";
            marker_pos = Vector3(dash_start.x, 0.0, dash_start.z);
            marker_dir = dash_dir;
            marker_length = THRUST_LENGTH;
            marker_radius = THRUST_WIDTH;
            if (mode_time >= 0.82) {
                dash_start = get_global_position();
                set_mode(MODE_THRUST_DASH);
            }
        } break;

        case MODE_THRUST_DASH: {
            const double speed = 52.0;
            set_global_position(get_global_position() + dash_dir * speed * delta);
            apply_aim(dash_dir, delta, 30.0);
            const Vector3 sp = get_global_position();
            if (!hit_done) {
                // Расстояние от героя до отрезка траектории клинка (по плоскости).
                Vector3 rel = pp - Vector3(sp.x, 0.0, sp.z);
                rel.y = 0.0;
                if (rel.length() <= THRUST_WIDTH + 0.3) {
                    hurt_player(player, THRUST_DAMAGE);
                    hit_done = true;
                }
            }
            Vector3 travelled = sp - dash_start;
            travelled.y = 0.0;
            if (travelled.length() >= THRUST_LENGTH) set_mode(MODE_RETURN);
        } break;

        case MODE_SPIN: {
            const double wind = std::min(1.0, mode_time / 0.7);
            const double radius = 2.6 + (SPIN_RADIUS - 2.6) * wind;
            const double speed = 3.0 + 4.2 * wind;
            const Vector3 p = orbit_position(radius, 1.0, speed, delta);
            set_global_position(p);
            // Остриё по касательной: клинок "косит" вокруг босса.
            const Vector3 center = boss->get_global_position();
            Vector3 radial = p - center;
            radial.y = 0.0;
            apply_aim(Vector3(-radial.z, 0.0, radial.x), delta, 18.0);
            marker_kind = "ring";
            marker_pos = Vector3(center.x, 0.0, center.z);
            marker_radius = SPIN_RADIUS;
            if (mode_time > 0.7 && tick_cd <= 0.0) {
                Vector3 off = pp - Vector3(p.x, 0.0, p.z);
                off.y = 0.0;
                if (off.length() <= 1.25) {
                    hurt_player(player, SPIN_DAMAGE);
                    tick_cd = 0.45;
                }
            }
            if (mode_time >= 2.9) set_mode(MODE_RETURN);
        } break;

        case MODE_RETURN: {
            const Vector3 p = orbit_position(3.1, 1.7, 1.6, delta);
            set_global_position(get_global_position().lerp(p, 1.0 - std::exp(-5.5 * delta)));
            apply_aim(pp + Vector3(0.0, 1.0, 0.0) - get_global_position(), delta, 8.0);
            if (mode_time >= 0.55) set_mode(MODE_ORBIT);
        } break;

        default: break;
    }
}

String KladenetsSword3D::get_mode_name() const {
    switch (mode) {
        case MODE_ORBIT: return "orbit";
        case MODE_DIVE_RISE: return "dive_rise";
        case MODE_DIVE_PLUNGE: return "dive_plunge";
        case MODE_DIVE_STUCK: return "dive_stuck";
        case MODE_THRUST_AIM: return "thrust_aim";
        case MODE_THRUST_DASH: return "thrust_dash";
        case MODE_SPIN: return "spin";
        case MODE_RETURN: return "return";
        default: return "dead";
    }
}

String KladenetsSword3D::get_marker_kind() const { return marker_kind; }
Vector3 KladenetsSword3D::get_marker_position() const { return marker_pos; }
Vector3 KladenetsSword3D::get_marker_direction() const { return marker_dir; }
double KladenetsSword3D::get_marker_length() const { return marker_length; }
double KladenetsSword3D::get_marker_radius() const { return marker_radius; }

#include <algorithm>

#include "projectile_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>

using namespace godot;

void Projectile3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "params"), &Projectile3D::configure);
    ClassDB::bind_method(D_METHOD("get_kind"), &Projectile3D::get_kind);
    ClassDB::bind_method(D_METHOD("is_landed"), &Projectile3D::is_landed);
    ADD_SIGNAL(MethodInfo("landed",
            PropertyInfo(Variant::STRING, "kind"),
            PropertyInfo(Variant::VECTOR3, "position")));
}

void Projectile3D::configure(const Dictionary &p_params) {
    kind = String(p_params.get("kind", kind));
    arc_mode = bool(p_params.get("arc", arc_mode));
    from_pos = Vector3(p_params.get("from", from_pos));
    to_pos = Vector3(p_params.get("to", to_pos));
    flight_time = double(p_params.get("flight_time", flight_time));
    arc_height = double(p_params.get("arc_height", arc_height));
    direction = Vector3(p_params.get("direction", direction));
    speed = double(p_params.get("speed", speed));
    max_distance = double(p_params.get("max_distance", max_distance));
    damage = double(p_params.get("damage", damage));
    hit_radius = double(p_params.get("radius", hit_radius));
    homing = double(p_params.get("homing", homing));
    stun_time = double(p_params.get("stun", stun_time));
    slow_factor = double(p_params.get("slow_factor", slow_factor));
    slow_time = double(p_params.get("slow_time", slow_time));
    weaken_mult = double(p_params.get("weaken_mult", weaken_mult));
    weaken_time = double(p_params.get("weaken_time", weaken_time));
    vuln_mult = double(p_params.get("vuln_mult", vuln_mult));
    vuln_time = double(p_params.get("vuln_time", vuln_time));
    silence_time = double(p_params.get("silence_time", silence_time));

    direction.y = 0.0;
    if (direction.length_squared() > 0.0001) direction = direction.normalized();
    set_position(from_pos);
}

void Projectile3D::_physics_process(double delta) {
    if (has_landed) return;

    if (arc_mode) {
        elapsed += delta;
        const double t = std::clamp(elapsed / std::max(0.01, flight_time), 0.0, 1.0);
        Vector3 pos = from_pos.lerp(to_pos, t);
        pos.y += arc_height * 4.0 * t * (1.0 - t);
        set_global_position(pos);
        if (t >= 1.0) land();
        return;
    }

    if (kind == String("bolt")) {
        update_bolt(delta);
        return;
    }

    // Прямой полёт (камень героя).
    const Vector3 step = direction * speed * delta;
    set_global_position(get_global_position() + step);
    traveled += step.length();

    const Array enemies = get_tree()->get_nodes_in_group(StringName("enemies"));
    for (int i = 0; i < enemies.size(); ++i) {
        Node3D *enemy = Object::cast_to<Node3D>(enemies[i]);
        if (enemy == nullptr || !enemy->has_method(StringName("take_damage"))) continue;

        double enemy_radius = 0.8;
        if (enemy->has_method(StringName("get_hit_radius"))) enemy_radius = double(enemy->call(StringName("get_hit_radius")));

        Vector3 offset = enemy->get_global_position() - get_global_position();
        offset.y = 0.0;
        if (offset.length() <= enemy_radius + hit_radius) {
            enemy->call(StringName("take_damage"), damage);
            if (enemy->has_method(StringName("apply_knockback"))) enemy->call(StringName("apply_knockback"), direction * 3.0);
            queue_free();
            return;
        }
    }

    if (traveled >= max_distance) queue_free();
}

// Снаряд босса: слабое самонаведение, урон + дебаффы. Дэш героя пропускает снаряд сквозь себя.
void Projectile3D::update_bolt(double delta) {
    Node3D *player = find_player_3d(this);
    if (player != nullptr && homing > 0.0) {
        Vector3 to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
        if (to_player.length_squared() > 0.0001 && direction.length_squared() > 0.0001) {
            const double angle = direction.signed_angle_to(to_player.normalized(), Vector3(0.0, 1.0, 0.0));
            const double max_turn = homing * delta;
            direction = direction.rotated(Vector3(0.0, 1.0, 0.0), std::clamp(angle, -max_turn, max_turn));
        }
    }

    const Vector3 step = direction * speed * delta;
    set_global_position(get_global_position() + step);
    traveled += step.length();

    if (player != nullptr) {
        Vector3 offset = player->get_global_position() - get_global_position();
        offset.y = 0.0;
        if (offset.length() <= hit_radius + 0.35) {
            const bool dashing = player->has_method(StringName("is_dashing")) && bool(player->call(StringName("is_dashing")));
            if (!dashing) {
                if (player->has_method(StringName("take_damage"))) player->call(StringName("take_damage"), damage);
                if (stun_time > 0.0 && player->has_method(StringName("apply_stun"))) player->call(StringName("apply_stun"), stun_time);
                if (slow_time > 0.0 && player->has_method(StringName("apply_slow"))) player->call(StringName("apply_slow"), slow_factor, slow_time);
                if (weaken_time > 0.0 && player->has_method(StringName("apply_weaken"))) player->call(StringName("apply_weaken"), weaken_mult, weaken_time);
                if (vuln_time > 0.0 && player->has_method(StringName("apply_vulnerable"))) player->call(StringName("apply_vulnerable"), vuln_mult, vuln_time);
                if (silence_time > 0.0 && player->has_method(StringName("apply_silence"))) player->call(StringName("apply_silence"), silence_time);
                queue_free();
                return;
            }
        }
    }
    if (traveled >= max_distance) queue_free();
}

void Projectile3D::land() {
    has_landed = true;
    const Vector3 pos = get_global_position();

    if (kind == String("spruce")) {
        // Ель втыкается в землю: наносит урон по месту падения и ждёт, пока босс её заберёт.
        Node3D *player = find_player_3d(this);
        if (player != nullptr && player->has_method(StringName("take_damage"))) {
            Vector3 offset = player->get_global_position() - pos;
            offset.y = 0.0;
            if (offset.length() <= hit_radius) player->call(StringName("take_damage"), damage);
        }
        add_to_group(StringName("spruce_pickup"));
        emit_signal(StringName("landed"), kind, pos);
    } else {
        emit_signal(StringName("landed"), kind, pos);
        queue_free();
    }
}

String Projectile3D::get_kind() const { return kind; }
bool Projectile3D::is_landed() const { return has_landed; }

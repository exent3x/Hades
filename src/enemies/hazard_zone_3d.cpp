#include <algorithm>

#include "hazard_zone_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>

using namespace godot;

void HazardZone3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "params"), &HazardZone3D::configure);
    ClassDB::bind_method(D_METHOD("get_state_name"), &HazardZone3D::get_state_name);
    ClassDB::bind_method(D_METHOD("get_progress"), &HazardZone3D::get_progress);
    ClassDB::bind_method(D_METHOD("get_radius"), &HazardZone3D::get_radius);
    ClassDB::bind_method(D_METHOD("get_kind"), &HazardZone3D::get_kind);
    ClassDB::bind_method(D_METHOD("is_triggered"), &HazardZone3D::is_triggered);
}

void HazardZone3D::configure(const Dictionary &p_params) {
    kind = String(p_params.get("kind", kind));
    radius = double(p_params.get("radius", radius));
    telegraph_time = double(p_params.get("telegraph", telegraph_time));
    active_time = double(p_params.get("active", active_time));
    damage = double(p_params.get("damage", damage));
    tick_interval = double(p_params.get("tick", tick_interval));
    stun_time = double(p_params.get("stun", stun_time));
    slow_factor = double(p_params.get("slow_factor", slow_factor));
    slow_time = double(p_params.get("slow_time", slow_time));
    weaken_mult = double(p_params.get("weaken_mult", weaken_mult));
    weaken_time = double(p_params.get("weaken_time", weaken_time));
    explodes = bool(p_params.get("explodes", explodes));

    timer = telegraph_time;
    zone_state = ZONE_TELEGRAPH;

    if (kind == String("root")) add_to_group(StringName("roots"));
    else if (kind == String("birds")) add_to_group(StringName("birds"));
    else if (kind == String("miasma")) add_to_group(StringName("miasma"));
    else if (kind == String("mushroom")) add_to_group(StringName("mushrooms"));
}

void HazardZone3D::_ready() {
    add_to_group(StringName("hazards"));
}

Node3D *HazardZone3D::get_player() const {
    if (!is_inside_tree()) return nullptr;
    return find_player_3d(this);
}

bool HazardZone3D::player_inside(Node3D *player, double extra) const {
    if (player == nullptr) return false;
    Vector3 offset = player->get_global_position() - get_global_position();
    offset.y = 0.0;
    return offset.length() <= radius + extra;
}

void HazardZone3D::explode(Node3D *player) {
    if (player == nullptr || !player_inside(player, 0.25)) return;
    if (player->has_method(StringName("take_damage"))) player->call(StringName("take_damage"), damage);
    if (player->has_method(StringName("apply_slow"))) player->call(StringName("apply_slow"), slow_factor, slow_time);
    if (player->has_method(StringName("apply_weaken"))) player->call(StringName("apply_weaken"), weaken_mult, weaken_time);
}

void HazardZone3D::_physics_process(double delta) {
    if (zone_state == ZONE_DONE) return;

    timer -= delta;
    Node3D *player = get_player();

    if (zone_state == ZONE_TELEGRAPH) {
        if (timer > 0.0) return;

        zone_state = ZONE_ACTIVE;
        timer = active_time;
        tick_left = 0.0;
        if (explodes) explode(player);
        return;
    }

    // ZONE_ACTIVE
    if (kind == String("root") && !triggered && player != nullptr && player_inside(player, 0.2)) {
        // Дэш позволяет перепрыгнуть корень: не срабатывает и не тратится.
        const bool dashing = player->has_method(StringName("is_dashing")) && bool(player->call(StringName("is_dashing")));
        if (!dashing) {
            triggered = true;
            if (player->has_method(StringName("take_damage"))) player->call(StringName("take_damage"), damage);
            if (player->has_method(StringName("apply_stun"))) player->call(StringName("apply_stun"), stun_time);
        }
    } else if (kind == String("birds") || kind == String("miasma")) {
        tick_left -= delta;
        if (tick_left <= 0.0) {
            tick_left = tick_interval;
            if (player != nullptr && player_inside(player, 0.2) && player->has_method(StringName("take_damage"))) {
                player->call(StringName("take_damage"), damage);
                // Ядовитая зона (miasma) ещё и замедляет, пока герой внутри.
                if (kind == String("miasma") && slow_time > 0.0 && player->has_method(StringName("apply_slow"))) {
                    player->call(StringName("apply_slow"), slow_factor, slow_time);
                }
            }
        }
    }

    if (timer <= 0.0) {
        zone_state = ZONE_DONE;
        queue_free();
    }
}

String HazardZone3D::get_state_name() const {
    switch (zone_state) {
        case ZONE_TELEGRAPH: return "telegraph";
        case ZONE_ACTIVE: return "active";
        default: return "done";
    }
}

double HazardZone3D::get_progress() const {
    const double total = (zone_state == ZONE_TELEGRAPH) ? telegraph_time : active_time;
    if (total <= 0.0) return 1.0;
    return std::clamp(1.0 - timer / total, 0.0, 1.0);
}

double HazardZone3D::get_radius() const { return radius; }
String HazardZone3D::get_kind() const { return kind; }
bool HazardZone3D::is_triggered() const { return triggered; }

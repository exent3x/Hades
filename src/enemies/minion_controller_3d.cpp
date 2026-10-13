#include <algorithm>
#include <cmath>

#include "minion_controller_3d.h"
#include "player_lookup_3d.h"

#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/color.hpp>

using namespace godot;

void MinionController3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("configure", "params"), &MinionController3D::configure);
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &MinionController3D::take_damage);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &MinionController3D::apply_knockback);
    ClassDB::bind_method(D_METHOD("get_health"), &MinionController3D::get_health);
    ClassDB::bind_method(D_METHOD("get_hit_radius"), &MinionController3D::get_hit_radius);
}

void MinionController3D::_ready() {
    add_to_group(StringName("enemies"));
    add_to_group(StringName("minions"));
    sprite = Object::cast_to<Sprite3D>(get_node_or_null(NodePath("Sprite")));
    health = max_health;
    set_motion_mode(CharacterBody3D::MOTION_MODE_FLOATING);
}

void MinionController3D::_physics_process(double delta) {
    attack_cooldown_left = std::max(0.0, attack_cooldown_left - delta);
    hit_flash_left = std::max(0.0, hit_flash_left - delta);
    knockback_velocity = knockback_velocity.move_toward(Vector3(), knockback_decay * delta);

    Node3D *player = find_player_3d(this);
    Vector3 desired;

    if (player != nullptr) {
        const bool player_dead = player->has_method(StringName("is_dead")) && bool(player->call(StringName("is_dead")));
        Vector3 to_player = player->get_global_position() - get_global_position();
        to_player.y = 0.0;
        const double distance = to_player.length();

        if (sprite != nullptr && std::fabs(to_player.x) > 0.2) sprite->set_flip_h(to_player.x < 0.0);

        // Приспешники тоже твёрдые для героя (слои разведены), выталкивается только герой.
        if (player->has_method(StringName("push_out"))) {
            const double min_dist = 0.55 + 0.3;
            if (distance < min_dist) {
                const Vector3 dir = (distance > 0.001) ? (to_player / distance) : Vector3(0.0, 0.0, 1.0);
                player->call(StringName("push_out"), dir * ((min_dist - distance) * std::min(1.0, delta * 12.0)));
            }
        }

        if (!player_dead) {
            if (distance > attack_range && distance > 0.001) desired = to_player.normalized() * move_speed;
            if (distance <= attack_range && attack_cooldown_left <= 0.0 && player->has_method(StringName("take_damage"))) {
                player->call(StringName("take_damage"), contact_damage);
                attack_cooldown_left = attack_cooldown;
            }
        }
    }

    set_velocity(desired + knockback_velocity);
    move_and_slide();

    if (sprite != nullptr) {
        sprite->set_modulate(hit_flash_left > 0.0 ? Color(1.6, 1.6, 1.6, 1.0) : Color(1.0, 1.0, 1.0, 1.0));
    }
}

// Варианты приспешников (лешонок / скелет): статы задаёт тот, кто их создаёт.
void MinionController3D::configure(const Dictionary &p_params) {
    max_health = double(p_params.get("hp", max_health));
    health = max_health;
    move_speed = double(p_params.get("speed", move_speed));
    contact_damage = double(p_params.get("damage", contact_damage));
    attack_cooldown = double(p_params.get("cooldown", attack_cooldown));
    attack_range = double(p_params.get("range", attack_range));
}

void MinionController3D::take_damage(double amount) {
    if (amount <= 0.0 || health <= 0.0) return;
    health = std::max(0.0, health - amount);
    hit_flash_left = 0.08;
    if (health <= 0.0) queue_free();
}

void MinionController3D::apply_knockback(const Vector3 &force) {
    if (health <= 0.0) return;
    Vector3 f = force;
    f.y = 0.0;
    knockback_velocity += f;
}

double MinionController3D::get_health() const { return health; }
double MinionController3D::get_hit_radius() const { return 0.55; }

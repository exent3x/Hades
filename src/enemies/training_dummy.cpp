#include <algorithm>

#include "training_dummy.h"

#include <godot_cpp/classes/character_body2d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node2d.hpp>
#include <godot_cpp/classes/polygon2d.hpp>
#include <godot_cpp/classes/progress_bar.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void TrainingDummy::_bind_methods() {
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &TrainingDummy::take_damage);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &TrainingDummy::apply_knockback);
    ClassDB::bind_method(D_METHOD("set_elite", "value"), &TrainingDummy::set_elite);
    ClassDB::bind_method(D_METHOD("is_elite"), &TrainingDummy::is_elite);
    ClassDB::bind_method(D_METHOD("set_boss", "value"), &TrainingDummy::set_boss);
    ClassDB::bind_method(D_METHOD("is_boss"), &TrainingDummy::is_boss);
    ClassDB::bind_method(D_METHOD("get_health"), &TrainingDummy::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &TrainingDummy::get_max_health);
}

void TrainingDummy::_ready() {
    add_to_group(StringName("enemies"));
    body_visual = Object::cast_to<Polygon2D>(get_node_or_null(NodePath("Body")));
    telegraph_visual = Object::cast_to<Polygon2D>(get_node_or_null(NodePath("Telegraph")));
    health_bar = Object::cast_to<ProgressBar>(get_node_or_null(NodePath("HPBar")));
    health = max_health;

    set_motion_mode(CharacterBody2D::MOTION_MODE_FLOATING);
    if (health_bar != nullptr) {
        health_bar->set_max(max_health);
        health_bar->set_value(health);
    }
    if (telegraph_visual != nullptr) telegraph_visual->set_visible(false);
}

void TrainingDummy::_physics_process(double delta) {
    attack_cooldown_left = std::max(0.0, attack_cooldown_left - delta);
    hit_flash_left = std::max(0.0, hit_flash_left - delta);
    knockback_velocity = knockback_velocity.move_toward(Vector2(), knockback_decay * delta);

    if (body_visual != nullptr) {
        body_visual->set_modulate(hit_flash_left > 0.0 ? Color(1.4, 1.4, 1.4, 1.0) : Color(1.0, 1.0, 1.0, 1.0));
        body_visual->set_scale(boss ? Vector2(1.35, 1.35) : (elite ? Vector2(1.18, 1.18) : Vector2(1.0, 1.0)));
    }

    if (boss) {
        update_boss_phase();
        update_boss_special(delta);
    }

    CharacterBody2D *player = Object::cast_to<CharacterBody2D>(
            get_tree()->get_first_node_in_group(StringName("player")));

    if (player == nullptr) {
        set_velocity(knockback_velocity);
        move_and_slide();
        return;
    }

    const Vector2 to_player = player->get_global_position() - get_global_position();
    const double distance = to_player.length();
    Vector2 desired_velocity = Vector2();

    if (distance > attack_range && distance > 0.001) {
        desired_velocity = to_player.normalized() * move_speed;
    }

    set_velocity(desired_velocity + knockback_velocity);
    move_and_slide();

    if (distance <= attack_range && attack_cooldown_left <= 0.0 && player->has_method(StringName("take_damage"))) {
        player->call(StringName("take_damage"), contact_damage);
        attack_cooldown_left = attack_cooldown;
    }
}

void TrainingDummy::update_boss_phase() {
    if (!boss || max_health <= 0.0) return;

    const double ratio = health / max_health;
    if (ratio > 0.66) {
        move_speed = 76.0;
        contact_damage = 18.0;
        special_radius = 175.0;
        special_damage = 24.0;
    } else if (ratio > 0.33) {
        move_speed = 92.0;
        contact_damage = 24.0;
        special_radius = 205.0;
        special_damage = 30.0;
    } else {
        move_speed = 112.0;
        contact_damage = 30.0;
        special_radius = 235.0;
        special_damage = 38.0;
    }

    if (body_visual != nullptr) {
        body_visual->set_color(ratio > 0.66 ? Color(0.34, 0.18, 0.08, 1.0)
                                            : (ratio > 0.33 ? Color(0.50, 0.13, 0.08, 1.0)
                                                            : Color(0.62, 0.08, 0.08, 1.0)));
    }
}

void TrainingDummy::update_boss_special(double delta) {
    if (!boss) return;

    if (special_windup_left > 0.0) {
        special_windup_left = std::max(0.0, special_windup_left - delta);
        if (telegraph_visual != nullptr) {
            telegraph_visual->set_visible(true);
            telegraph_visual->set_scale(Vector2(special_radius / 180.0, special_radius / 180.0));
        }
        if (special_windup_left <= 0.0) {
            CharacterBody2D *player = Object::cast_to<CharacterBody2D>(
                    get_tree()->get_first_node_in_group(StringName("player")));
            if (player != nullptr && player->has_method(StringName("take_damage"))) {
                const double distance = player->get_global_position().distance_to(get_global_position());
                if (distance <= special_radius) player->call(StringName("take_damage"), special_damage);
            }
            if (telegraph_visual != nullptr) telegraph_visual->set_visible(false);
        }
        return;
    }

    special_cooldown_left = std::max(0.0, special_cooldown_left - delta);
    if (special_cooldown_left <= 0.0) {
        special_windup_left = 0.48;
        special_cooldown_left = 1.9;
    }
}

void TrainingDummy::take_damage(double amount) {
    if (amount <= 0.0 || health <= 0.0) return;

    health = std::max(0.0, health - amount);
    hit_flash_left = 0.07;
    if (health_bar != nullptr) health_bar->set_value(health);
    if (health <= 0.0) queue_free();
}

void TrainingDummy::apply_knockback(const Vector2 &force) {
    if (health <= 0.0 || boss) return;
    knockback_velocity += force;
}

void TrainingDummy::set_elite(bool value) {
    elite = value;
    if (!elite || boss) return;
    max_health = 150.0;
    health = max_health;
    move_speed = 108.0;
    contact_damage = 16.0;
    attack_cooldown = 0.65;
    if (body_visual != nullptr) body_visual->set_color(Color(0.55, 0.20, 0.70, 1.0));
    if (health_bar != nullptr) {
        health_bar->set_max(max_health);
        health_bar->set_value(health);
    }
}

void TrainingDummy::set_boss(bool value) {
    boss = value;
    if (!boss) return;

    elite = false;
    max_health = 900.0;
    health = max_health;
    move_speed = 76.0;
    attack_range = 68.0;
    contact_damage = 18.0;
    attack_cooldown = 0.95;
    special_cooldown_left = 1.25;
    special_windup_left = 0.0;

    add_to_group(StringName("bosses"));

    if (body_visual != nullptr) body_visual->set_color(Color(0.34, 0.18, 0.08, 1.0));
    if (health_bar != nullptr) {
        health_bar->set_max(max_health);
        health_bar->set_value(health);
    }
}

bool TrainingDummy::is_elite() const { return elite; }
bool TrainingDummy::is_boss() const { return boss; }
double TrainingDummy::get_health() const { return health; }
double TrainingDummy::get_max_health() const { return max_health; }

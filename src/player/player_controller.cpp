#include <algorithm>

#include "player_controller.h"

#include <godot_cpp/classes/area2d.hpp>
#include <godot_cpp/classes/collision_shape2d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/polygon2d.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

void PlayerController::_bind_methods() {
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &PlayerController::take_damage);
    ClassDB::bind_method(D_METHOD("heal", "amount"), &PlayerController::heal);
    ClassDB::bind_method(D_METHOD("increase_attack", "percent"), &PlayerController::increase_attack);
    ClassDB::bind_method(D_METHOD("increase_max_health", "amount"), &PlayerController::increase_max_health);
    ClassDB::bind_method(D_METHOD("reduce_dash_cooldown", "amount"), &PlayerController::reduce_dash_cooldown);
    ClassDB::bind_method(D_METHOD("reset_run_stats"), &PlayerController::reset_run_stats);

    ClassDB::bind_method(D_METHOD("get_health"), &PlayerController::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &PlayerController::get_max_health);
    ClassDB::bind_method(D_METHOD("get_attack_damage_multiplier"), &PlayerController::get_attack_damage_multiplier);
    ClassDB::bind_method(D_METHOD("get_dash_cooldown"), &PlayerController::get_dash_cooldown);
    ClassDB::bind_method(D_METHOD("is_dashing"), &PlayerController::is_dashing);
    ClassDB::bind_method(D_METHOD("is_attacking"), &PlayerController::is_attacking);
    ClassDB::bind_method(D_METHOD("is_using_skill"), &PlayerController::is_using_skill);
    ClassDB::bind_method(D_METHOD("is_dead"), &PlayerController::is_dead);
    ADD_SIGNAL(MethodInfo("died"));
    ClassDB::bind_method(D_METHOD("get_combo_step"), &PlayerController::get_combo_step);
    ClassDB::bind_method(D_METHOD("get_facing"), &PlayerController::get_facing);
    ClassDB::bind_method(D_METHOD("get_dash_cooldown_left"), &PlayerController::get_dash_cooldown_left);
    ClassDB::bind_method(D_METHOD("get_skill_cooldown_left"), &PlayerController::get_skill_cooldown_left);
}

void PlayerController::_ready() {
    attack_area = Object::cast_to<Area2D>(get_node_or_null(NodePath("AttackArea")));
    if (attack_area != nullptr) {
        attack_shape = Object::cast_to<CollisionShape2D>(attack_area->get_node_or_null(NodePath("CollisionShape2D")));
        attack_visual = Object::cast_to<Polygon2D>(attack_area->get_node_or_null(NodePath("Polygon2D")));
    }

    skill_area = Object::cast_to<Area2D>(get_node_or_null(NodePath("SkillArea")));
    if (skill_area != nullptr) {
        skill_shape = Object::cast_to<CollisionShape2D>(skill_area->get_node_or_null(NodePath("CollisionShape2D")));
        skill_visual = Object::cast_to<Polygon2D>(skill_area->get_node_or_null(NodePath("Polygon2D")));
    }

    if (attack_shape != nullptr) attack_shape->set_disabled(true);
    if (attack_visual != nullptr) attack_visual->set_visible(false);
    if (skill_shape != nullptr) skill_shape->set_disabled(true);
    if (skill_visual != nullptr) skill_visual->set_visible(false);

    health = max_health;
    hit_invulnerability_left = 0.0;
    set_motion_mode(CharacterBody2D::MOTION_MODE_FLOATING);
    add_to_group(StringName("player"));
    update_aim();
}

void PlayerController::_physics_process(double delta) {
    if (dead) return;

    Input *input = Input::get_singleton();

    dash_cooldown_left = std::max(0.0, dash_cooldown_left - delta);
    attack_cooldown_left = std::max(0.0, attack_cooldown_left - delta);
    combo_time_left = std::max(0.0, combo_time_left - delta);
    skill_cooldown_left = std::max(0.0, skill_cooldown_left - delta);
    hit_invulnerability_left = std::max(0.0, hit_invulnerability_left - delta);

    if (combo_time_left <= 0.0) combo_step = 0;
    update_aim();

    if (attack_time_left > 0.0) {
        attack_time_left -= delta;
        attack_elapsed += delta;
        if (!attack_has_hit && attack_elapsed >= 0.035) {
            perform_attack_hit();
            attack_has_hit = true;
        }
        if (attack_time_left <= 0.0) {
            attack_time_left = 0.0;
            finish_attack();
        }
    }

    if (skill_time_left > 0.0) {
        skill_time_left -= delta;
        if (!skill_has_hit && skill_time_left <= skill_duration * 0.55) {
            perform_skill_hit();
            skill_has_hit = true;
        }
        if (skill_time_left <= 0.0) {
            skill_time_left = 0.0;
            finish_skill();
        }
    }

    Vector2 move_input = input->get_vector(
            StringName("move_left"), StringName("move_right"), StringName("move_up"), StringName("move_down"));

    if (dash_time_left > 0.0) {
        dash_time_left -= delta;
        set_velocity(dash_direction * dash_speed);
        move_and_slide();
        return;
    }

    if (input->is_action_just_pressed(StringName("dash")) && dash_cooldown_left <= 0.0) {
        Vector2 direction = move_input;
        if (direction.length_squared() < 0.001) direction = facing;
        start_dash(direction.normalized());
        return;
    }

    set_velocity(move_input * move_speed);
    move_and_slide();

    if (input->is_action_just_pressed(StringName("attack")) &&
            attack_cooldown_left <= 0.0 && attack_time_left <= 0.0 && skill_time_left <= 0.0) {
        start_attack();
    }

    if (input->is_action_just_pressed(StringName("skill_1")) &&
            skill_cooldown_left <= 0.0 && skill_time_left <= 0.0 && dash_time_left <= 0.0) {
        start_skill();
    }
}

void PlayerController::update_aim() {
    const Vector2 to_mouse = get_global_mouse_position() - get_global_position();
    if (to_mouse.length_squared() > 0.001) facing = to_mouse.normalized();

    if (attack_area != nullptr) {
        attack_area->set_position(facing * 42.0);
        attack_area->set_rotation(facing.angle());
    }
}

void PlayerController::start_dash(const Vector2 &direction) {
    dash_direction = direction.length_squared() > 0.001 ? direction.normalized() : facing;
    dash_time_left = dash_duration;
    dash_cooldown_left = dash_cooldown;
}

void PlayerController::start_attack() {
    combo_step = (combo_time_left > 0.0) ? (combo_step % 3) + 1 : 1;
    combo_time_left = combo_reset_time;
    attack_time_left = attack_duration;
    attack_cooldown_left = attack_cooldown;
    attack_elapsed = 0.0;
    attack_has_hit = false;

    if (attack_area != nullptr) {
        attack_area->set_position(facing * (40.0 + 3.0 * combo_step));
        attack_area->set_rotation(facing.angle());
    }
    if (attack_visual != nullptr) {
        attack_visual->set_scale(Vector2(1.0 + 0.12 * combo_step, 1.0));
        attack_visual->set_visible(true);
    }
    if (attack_shape != nullptr) attack_shape->set_disabled(false);
}

void PlayerController::finish_attack() {
    if (attack_shape != nullptr) attack_shape->set_disabled(true);
    if (attack_visual != nullptr) attack_visual->set_visible(false);
}

void PlayerController::perform_attack_hit() {
    if (attack_area == nullptr) return;

    const Array overlapping = attack_area->get_overlapping_areas();
    const double damage = get_current_attack_damage();
    const double knockback = get_current_attack_knockback();

    for (int i = 0; i < overlapping.size(); ++i) {
        Area2D *hurtbox = Object::cast_to<Area2D>(overlapping[i]);
        if (hurtbox == nullptr) continue;
        Node *target = hurtbox->get_parent();
        if (target == nullptr || !target->has_method(StringName("take_damage"))) continue;
        target->call(StringName("take_damage"), damage);
        if (target->has_method(StringName("apply_knockback"))) {
            target->call(StringName("apply_knockback"), facing * knockback);
        }
    }
}

void PlayerController::start_skill() {
    skill_time_left = skill_duration;
    skill_cooldown_left = skill_cooldown;
    skill_has_hit = false;
    if (skill_shape != nullptr) skill_shape->set_disabled(false);
    if (skill_visual != nullptr) skill_visual->set_visible(true);
}

void PlayerController::finish_skill() {
    if (skill_shape != nullptr) skill_shape->set_disabled(true);
    if (skill_visual != nullptr) skill_visual->set_visible(false);
}

void PlayerController::perform_skill_hit() {
    if (skill_area == nullptr) return;
    const Array overlapping = skill_area->get_overlapping_areas();
    for (int i = 0; i < overlapping.size(); ++i) {
        Area2D *hurtbox = Object::cast_to<Area2D>(overlapping[i]);
        if (hurtbox == nullptr) continue;
        Node *target = hurtbox->get_parent();
        if (target == nullptr || !target->has_method(StringName("take_damage"))) continue;
        target->call(StringName("take_damage"), skill_damage);
        if (target->has_method(StringName("apply_knockback"))) {
            Node2D *target_2d = Object::cast_to<Node2D>(target);
            if (target_2d != nullptr) {
                Vector2 away = target_2d->get_global_position() - get_global_position();
                if (away.length_squared() < 0.001) away = facing;
                target->call(StringName("apply_knockback"), away.normalized() * skill_knockback);
            }
        }
    }
}

double PlayerController::get_current_attack_damage() const {
    static const double combo_damage[3] = {20.0, 28.0, 42.0};
    const int index = std::clamp(combo_step, 1, 3) - 1;
    return combo_damage[index] * attack_damage_multiplier;
}

double PlayerController::get_current_attack_knockback() const {
    static const double combo_knockback[3] = {135.0, 185.0, 320.0};
    const int index = std::clamp(combo_step, 1, 3) - 1;
    return combo_knockback[index];
}

void PlayerController::take_damage(double amount) {
    if (amount <= 0.0 || dead || is_dashing() || hit_invulnerability_left > 0.0) return;

    health = std::max(0.0, health - amount);
    hit_invulnerability_left = 0.45;

    if (health <= 0.0) {
        health = 0.0;
        dead = true;
        set_velocity(Vector2());
        emit_signal(StringName("died"));
    }
}

void PlayerController::heal(double amount) {
    if (amount > 0.0) health = std::min(max_health, health + amount);
}

void PlayerController::increase_attack(double percent) {
    if (percent > 0.0) attack_damage_multiplier += percent;
}

void PlayerController::increase_max_health(double amount) {
    if (amount <= 0.0) return;
    max_health += amount;
    health = std::min(max_health, health + amount);
}

void PlayerController::reduce_dash_cooldown(double amount) {
    if (amount <= 0.0) return;
    dash_cooldown = std::max(0.22, dash_cooldown - amount);
}

void PlayerController::reset_run_stats() {
    dead = false;
    max_health = base_max_health;
    health = max_health;
    attack_damage_multiplier = 1.0;
    dash_cooldown = 0.60;
    dash_time_left = 0.0;
    dash_cooldown_left = 0.0;
    attack_time_left = 0.0;
    skill_time_left = 0.0;
    combo_time_left = 0.0;
    combo_step = 0;
}

double PlayerController::get_health() const { return health; }
double PlayerController::get_max_health() const { return max_health; }
double PlayerController::get_attack_damage_multiplier() const { return attack_damage_multiplier; }
double PlayerController::get_dash_cooldown() const { return dash_cooldown; }
bool PlayerController::is_dashing() const { return dash_time_left > 0.0; }
bool PlayerController::is_attacking() const { return attack_time_left > 0.0; }
bool PlayerController::is_using_skill() const { return skill_time_left > 0.0; }
bool PlayerController::is_dead() const { return dead; }
int PlayerController::get_combo_step() const { return combo_step; }
Vector2 PlayerController::get_facing() const { return facing; }
double PlayerController::get_dash_cooldown_left() const { return dash_cooldown_left; }
double PlayerController::get_skill_cooldown_left() const { return skill_cooldown_left; }

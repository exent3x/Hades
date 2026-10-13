#include <algorithm>
#include <cmath>

#include "player_controller_3d.h"

#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/input.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/sprite3d.hpp>
#include <godot_cpp/classes/viewport.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/color.hpp>

using namespace godot;

void PlayerController3D::_bind_methods() {
    ClassDB::bind_method(D_METHOD("take_damage", "amount"), &PlayerController3D::take_damage);
    ClassDB::bind_method(D_METHOD("heal", "amount"), &PlayerController3D::heal);
    ClassDB::bind_method(D_METHOD("increase_attack", "percent"), &PlayerController3D::increase_attack);
    ClassDB::bind_method(D_METHOD("increase_max_health", "amount"), &PlayerController3D::increase_max_health);
    ClassDB::bind_method(D_METHOD("reduce_dash_cooldown", "amount"), &PlayerController3D::reduce_dash_cooldown);
    ClassDB::bind_method(D_METHOD("reset_run_stats"), &PlayerController3D::reset_run_stats);

    ClassDB::bind_method(D_METHOD("get_health"), &PlayerController3D::get_health);
    ClassDB::bind_method(D_METHOD("get_max_health"), &PlayerController3D::get_max_health);
    ClassDB::bind_method(D_METHOD("get_attack_damage_multiplier"), &PlayerController3D::get_attack_damage_multiplier);
    ClassDB::bind_method(D_METHOD("get_dash_cooldown"), &PlayerController3D::get_dash_cooldown);
    ClassDB::bind_method(D_METHOD("is_dashing"), &PlayerController3D::is_dashing);
    ClassDB::bind_method(D_METHOD("is_attacking"), &PlayerController3D::is_attacking);
    ClassDB::bind_method(D_METHOD("is_using_skill"), &PlayerController3D::is_using_skill);
    ClassDB::bind_method(D_METHOD("is_dead"), &PlayerController3D::is_dead);
    ClassDB::bind_method(D_METHOD("get_combo_step"), &PlayerController3D::get_combo_step);
    ClassDB::bind_method(D_METHOD("get_facing"), &PlayerController3D::get_facing);
    ClassDB::bind_method(D_METHOD("get_dash_cooldown_left"), &PlayerController3D::get_dash_cooldown_left);
    ClassDB::bind_method(D_METHOD("get_skill_cooldown_left"), &PlayerController3D::get_skill_cooldown_left);
    ClassDB::bind_method(D_METHOD("apply_stun", "duration"), &PlayerController3D::apply_stun);
    ClassDB::bind_method(D_METHOD("apply_slow", "factor", "duration"), &PlayerController3D::apply_slow);
    ClassDB::bind_method(D_METHOD("apply_weaken", "multiplier", "duration"), &PlayerController3D::apply_weaken);
    ClassDB::bind_method(D_METHOD("apply_vulnerable", "multiplier", "duration"), &PlayerController3D::apply_vulnerable);
    ClassDB::bind_method(D_METHOD("apply_silence", "duration"), &PlayerController3D::apply_silence);
    ClassDB::bind_method(D_METHOD("get_vulnerable_left"), &PlayerController3D::get_vulnerable_left);
    ClassDB::bind_method(D_METHOD("get_silence_left"), &PlayerController3D::get_silence_left);
    ClassDB::bind_method(D_METHOD("apply_knockback", "force"), &PlayerController3D::apply_knockback);
    ClassDB::bind_method(D_METHOD("push_out", "offset"), &PlayerController3D::push_out);
    ClassDB::bind_method(D_METHOD("set_health", "value"), &PlayerController3D::set_health);
    ClassDB::bind_method(D_METHOD("get_stun_left"), &PlayerController3D::get_stun_left);
    ClassDB::bind_method(D_METHOD("get_slow_left"), &PlayerController3D::get_slow_left);
    ClassDB::bind_method(D_METHOD("get_weaken_left"), &PlayerController3D::get_weaken_left);
    ADD_SIGNAL(MethodInfo("died"));
    ADD_SIGNAL(MethodInfo("throw_requested",
            PropertyInfo(Variant::VECTOR3, "origin"),
            PropertyInfo(Variant::VECTOR3, "direction"),
            PropertyInfo(Variant::FLOAT, "damage")));
}

void PlayerController3D::_ready() {
    attack_area = Object::cast_to<Area3D>(get_node_or_null(NodePath("AttackArea")));
    if (attack_area != nullptr) {
        attack_shape = Object::cast_to<CollisionShape3D>(attack_area->get_node_or_null(NodePath("CollisionShape3D")));
        attack_visual = Object::cast_to<MeshInstance3D>(attack_area->get_node_or_null(NodePath("Visual")));
    }

    sprite = Object::cast_to<Sprite3D>(get_node_or_null(NodePath("Sprite")));
    visual = get_node_or_null(NodePath("Visual"));

    if (attack_shape != nullptr) attack_shape->set_disabled(true);
    if (attack_visual != nullptr) attack_visual->set_visible(false);

    health = max_health;
    hit_invulnerability_left = 0.0;
    set_motion_mode(CharacterBody3D::MOTION_MODE_FLOATING);
    add_to_group(StringName("player"));
    update_aim();
}

void PlayerController3D::_physics_process(double delta) {
    if (dead) return;

    Input *input = Input::get_singleton();

    dash_cooldown_left = std::max(0.0, dash_cooldown_left - delta);
    attack_cooldown_left = std::max(0.0, attack_cooldown_left - delta);
    combo_time_left = std::max(0.0, combo_time_left - delta);
    skill_cooldown_left = std::max(0.0, skill_cooldown_left - delta);
    hit_invulnerability_left = std::max(0.0, hit_invulnerability_left - delta);
    stun_time_left = std::max(0.0, stun_time_left - delta);
    stun_immunity_left = std::max(0.0, stun_immunity_left - delta);
    slow_time_left = std::max(0.0, slow_time_left - delta);
    weaken_time_left = std::max(0.0, weaken_time_left - delta);
    if (slow_time_left <= 0.0) slow_factor = 1.0;
    if (weaken_time_left <= 0.0) weaken_mult = 1.0;
    vuln_time_left = std::max(0.0, vuln_time_left - delta);
    silence_time_left = std::max(0.0, silence_time_left - delta);
    if (vuln_time_left <= 0.0) vuln_mult = 1.0;

    // Затухание импульса отбрасывания.
    if (knockback_velocity.length_squared() > 0.0001) {
        knockback_velocity = knockback_velocity.move_toward(Vector3(), knockback_decay * delta);
    } else {
        knockback_velocity = Vector3();
    }

    if (combo_time_left <= 0.0) combo_step = 0;
    update_aim();
    update_sprite();

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
        skill_time_left = std::max(0.0, skill_time_left - delta);
    }

    // Оглушение: герой стоит на месте, но импульс отбрасывания всё равно работает.
    // Это позволяет grab_throw донести его до стены, даже пока он в стане.
    if (stun_time_left > 0.0) {
        set_velocity(knockback_velocity);
        move_and_slide();
        return;
    }

    // Экранные "вверх/вниз" -> мировые -Z/+Z (камера смотрит в сторону -Z).
    const Vector2 raw = input->get_vector(
            StringName("move_left"), StringName("move_right"), StringName("move_up"), StringName("move_down"));
    const Vector3 move_input = Vector3(raw.x, 0.0, raw.y);

    if (dash_time_left > 0.0) {
        dash_time_left -= delta;
        set_velocity(dash_direction * dash_speed + knockback_velocity);
        move_and_slide();
        return;
    }

    if (input->is_action_just_pressed(StringName("dash")) && dash_cooldown_left <= 0.0) {
        Vector3 direction = move_input;
        if (direction.length_squared() < 0.001) direction = facing;
        start_dash(direction.normalized());
        return;
    }

    const double speed_mult = (slow_time_left > 0.0) ? slow_factor : 1.0;
    set_velocity(move_input * move_speed * speed_mult + knockback_velocity);
    move_and_slide();

    if (input->is_action_just_pressed(StringName("attack")) &&
            attack_cooldown_left <= 0.0 && attack_time_left <= 0.0 && skill_time_left <= 0.0) {
        start_attack();
    }

    if (input->is_action_just_pressed(StringName("skill_1")) && silence_time_left <= 0.0 &&
            skill_cooldown_left <= 0.0 && skill_time_left <= 0.0 && dash_time_left <= 0.0) {
        start_skill();
    }
}

void PlayerController3D::update_aim() {
    // Луч из камеры через курсор -> пересечение с плоскостью пола (y = высота ног игрока).
    Viewport *viewport = get_viewport();
    if (viewport != nullptr) {
        Camera3D *camera = viewport->get_camera_3d();
        if (camera != nullptr) {
            const Vector2 mouse = viewport->get_mouse_position();
            const Vector3 origin = camera->project_ray_origin(mouse);
            const Vector3 dir = camera->project_ray_normal(mouse);
            if (std::fabs(dir.y) > 0.0001) {
                const double t = (get_global_position().y - origin.y) / dir.y;
                if (t > 0.0) {
                    Vector3 to_mouse = (origin + dir * t) - get_global_position();
                    to_mouse.y = 0.0;
                    if (to_mouse.length_squared() > 0.0001) facing = to_mouse.normalized();
                }
            }
        }
    }

    if (attack_area != nullptr) {
        attack_area->set_position(facing * 0.7);
        attack_area->set_rotation(Vector3(0.0, std::atan2(-facing.z, facing.x), 0.0));
    }
}

void PlayerController3D::update_sprite() {
    // Оттенок по состоянию: удар > оглушение > замедление > ослабление.
    Color tint = Color(1.0, 1.0, 1.0, 1.0);
    if (hit_invulnerability_left > 0.0) tint = Color(1.0, 0.55, 0.55, 1.0);
    else if (stun_time_left > 0.0) tint = Color(1.0, 1.0, 0.45, 1.0);
    else if (slow_time_left > 0.0) tint = Color(0.65, 0.85, 1.0, 1.0);
    else if (vuln_time_left > 0.0) tint = Color(1.0, 0.72, 0.45, 1.0);
    else if (silence_time_left > 0.0) tint = Color(0.62, 0.62, 0.70, 1.0);
    else if (weaken_time_left > 0.0) tint = Color(0.85, 0.65, 1.0, 1.0);

    if (sprite != nullptr) {
        if (std::fabs(facing.x) > 0.15) sprite->set_flip_h(facing.x < 0.0);
        sprite->set_modulate(tint);
    }
    if (visual != nullptr && visual->has_method(StringName("set_tint"))) {
        visual->call(StringName("set_tint"), tint);
    }
}

void PlayerController3D::start_dash(const Vector3 &direction) {
    dash_direction = direction.length_squared() > 0.001 ? direction.normalized() : facing;
    dash_time_left = dash_duration;
    dash_cooldown_left = dash_cooldown;
}

void PlayerController3D::start_attack() {
    combo_step = (combo_time_left > 0.0) ? (combo_step % 3) + 1 : 1;
    combo_time_left = combo_reset_time;
    attack_time_left = attack_duration;
    attack_cooldown_left = attack_cooldown;
    attack_elapsed = 0.0;
    attack_has_hit = false;

    if (attack_area != nullptr) {
        attack_area->set_position(facing * (0.66 + 0.05 * combo_step));
        attack_area->set_rotation(Vector3(0.0, std::atan2(-facing.z, facing.x), 0.0));
    }
    if (attack_visual != nullptr) {
        attack_visual->set_scale(Vector3(1.0 + 0.12 * combo_step, 1.0, 1.0));
        attack_visual->set_visible(true);
    }
    if (attack_shape != nullptr) attack_shape->set_disabled(false);
}

void PlayerController3D::finish_attack() {
    if (attack_shape != nullptr) attack_shape->set_disabled(true);
    if (attack_visual != nullptr) attack_visual->set_visible(false);
}

void PlayerController3D::perform_attack_hit() {
    if (attack_area == nullptr) return;

    const Array overlapping = attack_area->get_overlapping_areas();
    const double damage = get_current_attack_damage() * weaken_mult;
    const double knockback = get_current_attack_knockback();

    for (int i = 0; i < overlapping.size(); ++i) {
        Area3D *hurtbox = Object::cast_to<Area3D>(overlapping[i]);
        if (hurtbox == nullptr) continue;
        Node *target = hurtbox->get_parent();
        if (target == nullptr || !target->has_method(StringName("take_damage"))) continue;
        target->call(StringName("take_damage"), damage);
        if (target->has_method(StringName("apply_knockback"))) {
            target->call(StringName("apply_knockback"), facing * knockback);
        }
    }
}

void PlayerController3D::start_skill() {
    skill_time_left = skill_duration;
    skill_cooldown_left = skill_cooldown;
    // Сам камень создаёт GDScript-обёртка (визуал + Projectile3D) по сигналу.
    const Vector3 origin = get_global_position() + Vector3(0.0, 1.0, 0.0) + facing * 0.5;
    emit_signal(StringName("throw_requested"), origin, facing, skill_damage * weaken_mult);
}

double PlayerController3D::get_current_attack_damage() const {
    static const double combo_damage[3] = { 20.0, 28.0, 42.0 };
    const int index = std::clamp(combo_step, 1, 3) - 1;
    return combo_damage[index] * attack_damage_multiplier;
}

double PlayerController3D::get_current_attack_knockback() const {
    static const double combo_knockback[3] = { 2.3, 3.1, 5.3 };
    const int index = std::clamp(combo_step, 1, 3) - 1;
    return combo_knockback[index];
}

void PlayerController3D::take_damage(double amount) {
    if (amount <= 0.0 || dead || is_dashing() || hit_invulnerability_left > 0.0) return;

    if (vuln_time_left > 0.0) amount *= vuln_mult;
    health = std::max(0.0, health - amount);
    hit_invulnerability_left = 0.45;

    if (health <= 0.0) {
        health = 0.0;
        dead = true;
        set_velocity(Vector3());
        emit_signal(StringName("died"));
    }
}

void PlayerController3D::heal(double amount) {
    if (amount > 0.0) health = std::min(max_health, health + amount);
}

void PlayerController3D::increase_attack(double percent) {
    if (percent > 0.0) attack_damage_multiplier += percent;
}

void PlayerController3D::increase_max_health(double amount) {
    if (amount <= 0.0) return;
    max_health += amount;
    health = std::min(max_health, health + amount);
}

void PlayerController3D::reduce_dash_cooldown(double amount) {
    if (amount <= 0.0) return;
    dash_cooldown = std::max(0.22, dash_cooldown - amount);
}

void PlayerController3D::reset_run_stats() {
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
    stun_time_left = 0.0;
    stun_immunity_left = 0.0;
    slow_time_left = 0.0;
    slow_factor = 1.0;
    weaken_time_left = 0.0;
    vuln_time_left = 0.0;
    vuln_mult = 1.0;
    silence_time_left = 0.0;
    weaken_mult = 1.0;
    knockback_velocity = Vector3();
}

void PlayerController3D::apply_stun(double duration) {
    // Дэш даёт неуязвимость к оглушению; после оглушения — короткий иммунитет.
    if (duration <= 0.0 || dead || is_dashing() || stun_immunity_left > 0.0 || stun_time_left > 0.0) return;
    stun_time_left = duration;
    stun_immunity_left = duration + 0.7;
    dash_time_left = 0.0;
    attack_time_left = 0.0;
    finish_attack();
}

void PlayerController3D::apply_slow(double factor, double duration) {
    if (duration <= 0.0 || dead) return;
    slow_factor = (slow_time_left > 0.0) ? std::min(slow_factor, factor) : factor;
    slow_time_left = std::max(slow_time_left, duration);
}

void PlayerController3D::apply_vulnerable(double multiplier, double duration) {
    if (duration <= 0.0 || dead) return;
    vuln_mult = (vuln_time_left > 0.0) ? std::max(vuln_mult, multiplier) : multiplier;
    vuln_time_left = std::max(vuln_time_left, duration);
}

void PlayerController3D::apply_silence(double duration) {
    if (duration <= 0.0 || dead) return;
    silence_time_left = std::max(silence_time_left, duration);
}

double PlayerController3D::get_vulnerable_left() const { return vuln_time_left; }
double PlayerController3D::get_silence_left() const { return silence_time_left; }

void PlayerController3D::apply_weaken(double multiplier, double duration) {
    if (duration <= 0.0 || dead) return;
    weaken_mult = (weaken_time_left > 0.0) ? std::min(weaken_mult, multiplier) : multiplier;
    weaken_time_left = std::max(weaken_time_left, duration);
}

void PlayerController3D::push_out(const Vector3 &offset) {
    if (dead) return;
    Vector3 o = offset;
    o.y = 0.0;
    if (o.length_squared() < 1e-8) return;
    // move_and_collide уважает стены: игрока можно вытолкнуть, но не сквозь стену.
    move_and_collide(o);
}

void PlayerController3D::set_health(double value) {
    health = std::clamp(value, 1.0, max_health);
}

void PlayerController3D::apply_knockback(const Vector3 &force) {
    if (dead) return;
    // Y игнорируем — движение строго по плоскости XZ.
    Vector3 f = force;
    f.y = 0.0;
    // Суммируем, но с ограничением сверху, чтобы несколько импульсов не накапливались в 100 м/с.
    knockback_velocity += f;
    const double max_speed = 40.0;
    if (knockback_velocity.length() > max_speed) {
        knockback_velocity = knockback_velocity.normalized() * max_speed;
    }
}

double PlayerController3D::get_stun_left() const { return stun_time_left; }
double PlayerController3D::get_slow_left() const { return slow_time_left; }
double PlayerController3D::get_weaken_left() const { return weaken_time_left; }

double PlayerController3D::get_health() const { return health; }
double PlayerController3D::get_max_health() const { return max_health; }
double PlayerController3D::get_attack_damage_multiplier() const { return attack_damage_multiplier; }
double PlayerController3D::get_dash_cooldown() const { return dash_cooldown; }
bool PlayerController3D::is_dashing() const { return dash_time_left > 0.0; }
bool PlayerController3D::is_attacking() const { return attack_time_left > 0.0; }
bool PlayerController3D::is_using_skill() const { return skill_time_left > 0.0; }
bool PlayerController3D::is_dead() const { return dead; }
int PlayerController3D::get_combo_step() const { return combo_step; }
Vector3 PlayerController3D::get_facing() const { return facing; }
double PlayerController3D::get_dash_cooldown_left() const { return dash_cooldown_left; }
double PlayerController3D::get_skill_cooldown_left() const { return skill_cooldown_left; }
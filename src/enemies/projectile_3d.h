#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Универсальный снаряд.
//  "stone"  — прямой полёт от героя, бьёт первого врага из группы "enemies";
//  "spruce" — навесом в точку, бьёт героя при приземлении, затем остаётся в земле (группа "spruce_pickup");
//  "mushroom" — навесом в точку, при приземлении сообщает боссу (гриб вырастет сам).
// Только логика; внешний вид строит GDScript.
class Projectile3D : public Node3D {
    GDCLASS(Projectile3D, Node3D)

private:
    String kind = "stone";
    bool arc_mode = false;
    bool has_landed = false;

    Vector3 from_pos;
    Vector3 to_pos;
    double flight_time = 0.7;
    double elapsed = 0.0;
    double arc_height = 2.0;

    Vector3 direction = Vector3(1.0, 0.0, 0.0);
    double speed = 16.0;
    double max_distance = 14.0;
    double traveled = 0.0;

    double damage = 10.0;
    double hit_radius = 0.3;

    // "bolt": снаряд босса (самонаведение + дебаффы героя)
    double homing = 0.0;          // рад/с
    double stun_time = 0.0;
    double slow_factor = 1.0;
    double slow_time = 0.0;
    double weaken_mult = 1.0;
    double weaken_time = 0.0;
    double vuln_mult = 1.0;
    double vuln_time = 0.0;
    double silence_time = 0.0;
    void update_bolt(double delta);

    void land();

protected:
    static void _bind_methods();

public:
    void configure(const Dictionary &p_params);
    void _physics_process(double delta) override;

    String get_kind() const;
    bool is_landed() const;
};

} // namespace godot

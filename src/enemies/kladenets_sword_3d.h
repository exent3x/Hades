#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

// Меч-кладенец Кощея: летает по арене сам, но подчиняется хозяину (группа "koschei").
// Остриё клинка — локальная ось +Z (так модель подготовлена в koschei.glb).
// Режимы: орбита вокруг хозяина, удар сверху (dive), выпад по прямой (thrust), вращающийся клинок (spin).
class KladenetsSword3D : public Node3D {
    GDCLASS(KladenetsSword3D, Node3D)

public:
    enum Mode {
        MODE_ORBIT,
        MODE_DIVE_RISE,
        MODE_DIVE_PLUNGE,
        MODE_DIVE_STUCK,
        MODE_THRUST_AIM,
        MODE_THRUST_DASH,
        MODE_SPIN,
        MODE_RETURN,
        MODE_DEAD
    };

private:
    Mode mode = MODE_ORBIT;
    double mode_time = 0.0;
    double attack_cd = 3.0;
    double orbit_angle = 0.0;
    double tick_cd = 0.0;
    double time_alive = 0.0;

    Vector3 target_pos;
    Vector3 dash_dir = Vector3(0.0, 0.0, 1.0);
    Vector3 dash_start;
    Vector3 aim_dir = Vector3(0.0, 0.0, 1.0);
    bool hit_done = false;

    // для визуальных маркеров (читает GDScript)
    String marker_kind = "none";
    Vector3 marker_pos;
    Vector3 marker_dir = Vector3(0.0, 0.0, 1.0);
    double marker_length = 0.0;
    double marker_radius = 0.0;

    void set_mode(Mode m);
    Node3D *get_boss() const;
    double boss_phase() const;
    double damage_scale() const;
    bool boss_shielded() const;
    void apply_aim(const Vector3 &dir, double delta, double sharpness);
    Vector3 orbit_position(double radius, double height, double speed, double delta);
    void choose_attack(Node3D *player, Node3D *boss);
    void hurt_player(Node3D *player, double amount);

protected:
    static void _bind_methods();

public:
    void _ready() override;
    void _physics_process(double delta) override;

    void command(const String &name);
    String get_mode_name() const;
    String get_marker_kind() const;
    Vector3 get_marker_position() const;
    Vector3 get_marker_direction() const;
    double get_marker_length() const;
    double get_marker_radius() const;
};

} // namespace godot

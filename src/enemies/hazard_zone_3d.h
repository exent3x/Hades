#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/string.hpp>

namespace godot {

// Зона опасности на полу для босса: корни (урон + оглушение), рой птиц (урон по области),
// гриб (растёт, потом взрывается: урон + замедление + ослабление).
// Только логика; внешний вид строит GDScript и читает get_state_name()/get_progress().
class HazardZone3D : public Node3D {
    GDCLASS(HazardZone3D, Node3D)

public:
    enum ZoneState {
        ZONE_TELEGRAPH,
        ZONE_ACTIVE,
        ZONE_DONE
    };

private:
    String kind = "root";
    double radius = 1.0;
    double telegraph_time = 0.8;
    double active_time = 5.0;
    double damage = 10.0;
    double tick_interval = 0.5;
    double stun_time = 0.0;
    double slow_factor = 1.0;
    double slow_time = 0.0;
    double weaken_mult = 1.0;
    double weaken_time = 0.0;
    bool explodes = false;

    ZoneState zone_state = ZONE_TELEGRAPH;
    double timer = 0.8;
    double tick_left = 0.0;
    bool triggered = false;

    Node3D *get_player() const;
    bool player_inside(Node3D *player, double extra) const;
    void explode(Node3D *player);

protected:
    static void _bind_methods();

public:
    void configure(const Dictionary &p_params);

    void _ready() override;
    void _physics_process(double delta) override;

    String get_state_name() const;
    double get_progress() const;
    double get_radius() const;
    String get_kind() const;
    bool is_triggered() const;
};

} // namespace godot

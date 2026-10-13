#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/variant/array.hpp>

namespace godot {

// Ищет 3D-героя среди узлов группы "player".
// В основной игре в этой группе одновременно 2D-герой (PlayerController) и 3D-герой (PlayerController3D),
// а get_first_node_in_group() возвращал 2D-героя -> приведение к Node3D давало nullptr, и босс "ничего не делал".
inline Node3D *find_player_3d(const Node *from) {
    if (from == nullptr || !from->is_inside_tree()) return nullptr;
    const Array nodes = from->get_tree()->get_nodes_in_group(StringName("player"));
    for (int i = 0; i < nodes.size(); ++i) {
        Node3D *candidate = Object::cast_to<Node3D>(nodes[i]);
        if (candidate != nullptr && candidate->is_inside_tree()) return candidate;
    }
    return nullptr;
}

} // namespace godot

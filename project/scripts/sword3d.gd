extends Node3D
## Обёртка вокруг C++ KladenetsSword3D: меч-кладенец Кощея.
## Меш берётся из модели Кощея (koschei.glb, узел "Sword"); остриё — +Z.
## Телеграфы атак меча (круг удара сверху, полоса выпада, кольцо вращения) рисуются здесь по состоянию ядра.

const Util := preload("res://scripts/billboard_util.gd")

## Визуал Кощея (model_visual.gd) — источник меша меча. Задаётся до add_child().
var mesh_source = null
var core: Node3D = null
var circle_marker: MeshInstance3D
var line_marker: MeshInstance3D
var ring_marker: MeshInstance3D

func _ready() -> void:
    if not ClassDB.class_exists("KladenetsSword3D"):
        push_error("KladenetsSword3D не найден. Пересобери GDExtension (scons).")
        return
    core = ClassDB.instantiate("KladenetsSword3D") as Node3D
    if core == null:
        return
    add_child(core)

    var blade: Node3D = null
    if mesh_source != null:
        blade = mesh_source.call("make_sword_clone") as Node3D
    if blade == null:
        # Запасной клинок, если модели нет: тонкая длинная пластина остриём по +Z.
        var box := BoxMesh.new()
        box.size = Vector3(0.28, 0.06, 2.0)
        var mat := StandardMaterial3D.new()
        mat.albedo_color = Color(0.7, 0.72, 0.8)
        mat.metallic = 0.6
        box.material = mat
        var mi := MeshInstance3D.new()
        mi.mesh = box
        blade = Node3D.new()
        blade.add_child(mi)
    core.add_child(blade)

    # Холодное свечение клинка.
    var glow := OmniLight3D.new()
    glow.light_color = Color(0.55, 0.45, 1.0)
    glow.light_energy = 1.4
    glow.omni_range = 5.0
    core.add_child(glow)

    circle_marker = Util.make_disc(Color(0.9, 0.15, 0.2, 0.30), 0.05)
    circle_marker.visible = false
    add_child(circle_marker)
    ring_marker = Util.make_disc(Color(0.75, 0.2, 0.9, 0.10), 0.04)
    ring_marker.visible = false
    add_child(ring_marker)

    var plane := PlaneMesh.new()
    plane.size = Vector2(1.0, 1.0)
    plane.center_offset = Vector3(0.5, 0.0, 0.0)
    var line_mat := StandardMaterial3D.new()
    line_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    line_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    line_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    line_mat.albedo_color = Color(0.95, 0.2, 0.35, 0.30)
    plane.material = line_mat
    line_marker = MeshInstance3D.new()
    line_marker.mesh = plane
    line_marker.visible = false
    add_child(line_marker)

func place(pos: Vector3) -> void:
    if core != null:
        core.global_position = pos

func _process(_delta: float) -> void:
    if core == null or not is_instance_valid(core):
        return
    var kind: String = core.call("get_marker_kind")
    var pos: Vector3 = core.call("get_marker_position")
    var radius: float = core.call("get_marker_radius")

    circle_marker.visible = kind == "circle"
    ring_marker.visible = kind == "ring"
    line_marker.visible = kind == "line"

    if kind == "circle":
        circle_marker.global_position = Vector3(pos.x, 0.05, pos.z)
        circle_marker.scale = Vector3(radius, 1.0, radius)
    elif kind == "ring":
        ring_marker.global_position = Vector3(pos.x, 0.04, pos.z)
        ring_marker.scale = Vector3(radius, 1.0, radius)
    elif kind == "line":
        var dir: Vector3 = core.call("get_marker_direction")
        var length: float = core.call("get_marker_length")
        line_marker.global_position = Vector3(pos.x, 0.06, pos.z)
        line_marker.rotation = Vector3(0.0, atan2(-dir.z, dir.x), 0.0)
        line_marker.scale = Vector3(length, 1.0, radius * 2.0)

extends Node3D
## Обёртка вокруг C++ EnemyUnit3D: обычные враги 3D-комнат (brute / raider / jumper / archer, обычные и элитные).
## Строит коллизии, хёртбокс, спрайт-билборд, полосу HP и телеграфы (C++ сам их показывает и масштабирует).

const Util := preload("res://scripts/billboard_util.gd")

# роль -> [спрайт, высота (м), радиус тела, цвет ауры элиты]
const ROLES := {
    "brute": ["res://art/sprites/enemy_brute.png", 2.1, 0.75, Color(0.95, 0.25, 0.15)],
    "raider": ["res://art/sprites/enemy_raider.png", 1.7, 0.42, Color(0.65, 0.35, 0.95)],
    "jumper": ["res://art/sprites/enemy_jumper.png", 1.2, 0.45, Color(1.0, 0.55, 0.1)],
    "archer": ["res://art/sprites/enemy_archer.png", 1.8, 0.42, Color(0.2, 0.85, 0.95)],
}

## Задаются до add_child().
var role := "brute"
var elite := false
var hp_mult := 1.0
var dmg_mult := 1.0
var arena_half := Vector2(7.5, 5.0)

var core: CharacterBody3D = null

signal spawn_requested(kind: String, position: Vector3, data: Dictionary)

func _ready() -> void:
    if not ClassDB.class_exists("EnemyUnit3D"):
        push_error("EnemyUnit3D не найден. Пересобери GDExtension (scons).")
        return
    core = ClassDB.instantiate("EnemyUnit3D") as CharacterBody3D
    if core == null:
        return
    core.call("configure", {"role": role, "elite": elite, "hp_mult": hp_mult, "dmg_mult": dmg_mult})
    core.call("set_arena_half", arena_half)
    _build(core)
    add_child(core)
    core.position = Vector3.ZERO
    core.connect("spawn_requested", func(kind, pos, data): spawn_requested.emit(kind, pos, data))

func _build(body: CharacterBody3D) -> void:
    var info: Array = ROLES.get(role, ROLES["brute"])
    var k := 1.3 if elite else 1.0
    var height: float = float(info[1]) * k
    var radius: float = float(info[2]) * k

    body.collision_layer = 8
    body.collision_mask = 1

    body.add_child(Util.make_shadow(radius * 1.3, 0.5))
    if elite:
        # аура элиты: цветное кольцо под ногами
        var aura := Util.make_disc(Color(info[3].r, info[3].g, info[3].b, 0.35), 0.03)
        aura.scale = Vector3(radius * 1.5, 1.0, radius * 1.5)
        body.add_child(aura)
    body.add_child(Util.make_sprite(String(info[0]), height))

    var shape_node := CollisionShape3D.new()
    shape_node.name = "CollisionShape3D"
    var capsule := CapsuleShape3D.new()
    capsule.radius = radius * 0.8
    capsule.height = maxf(height * 0.8, radius * 2.2)
    shape_node.shape = capsule
    shape_node.position = Vector3(0, capsule.height * 0.5, 0)
    body.add_child(shape_node)

    # Хёртбокс: в него попадает AttackArea героя.
    var hurtbox := Area3D.new()
    hurtbox.name = "Hurtbox"
    hurtbox.collision_layer = 4
    hurtbox.collision_mask = 2
    body.add_child(hurtbox)
    var hurt_shape := CollisionShape3D.new()
    hurt_shape.name = "CollisionShape3D"
    var sphere := SphereShape3D.new()
    sphere.radius = radius * 1.1
    hurt_shape.shape = sphere
    hurt_shape.position = Vector3(0, height * 0.5, 0)
    hurtbox.add_child(hurt_shape)

    _build_hp_bar(body, height, radius, info[3])
    _build_telegraphs(body)

func _build_hp_bar(body: Node3D, height: float, radius: float, elite_color: Color) -> void:
    var root := Node3D.new()
    root.name = "HPBar"
    root.position = Vector3(0, height + 0.35, 0)
    root.scale = Vector3.ONE * clampf(radius * 2.4, 0.9, 1.7)
    root.visible = false
    body.add_child(root)
    root.add_child(_bar_quad("Back", Color(0.05, 0.03, 0.03, 0.85), 0.0))
    var fill := _bar_quad("Fill", elite_color if elite else Color(0.85, 0.15, 0.12, 1.0), 0.001)
    root.add_child(fill)

func _bar_quad(node_name: String, color: Color, z: float) -> MeshInstance3D:
    var quad := QuadMesh.new()
    quad.size = Vector2(1.0, 0.11)
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
    mat.albedo_color = color
    mat.no_depth_test = true
    mat.render_priority = 2 if node_name == "Fill" else 1
    quad.material = mat
    var mi := MeshInstance3D.new()
    mi.name = node_name
    mi.mesh = quad
    mi.position = Vector3(0, 0, z)
    return mi

func _build_telegraphs(body: Node3D) -> void:
    body.add_child(_plane("TeleCone", Vector3(0.5, 0.0, 0.0), Color(0.9, 0.15, 0.1, 0.30)))
    body.add_child(_plane("TeleLine", Vector3(0.5, 0.0, 0.0), Color(1.0, 0.35, 0.15, 0.40)))
    var disc := Util.make_disc(Color(0.95, 0.2, 0.1, 0.30), 0.05)
    disc.name = "TeleDisc"
    disc.visible = false
    body.add_child(disc)

func _plane(node_name: String, center_offset: Vector3, color: Color) -> MeshInstance3D:
    var plane := PlaneMesh.new()
    plane.size = Vector2(1.0, 1.0)
    plane.center_offset = center_offset
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    mat.cull_mode = BaseMaterial3D.CULL_DISABLED
    mat.albedo_color = color
    plane.material = mat
    var mi := MeshInstance3D.new()
    mi.name = node_name
    mi.mesh = plane
    mi.position = Vector3(0, 0.06, 0)
    mi.visible = false
    return mi

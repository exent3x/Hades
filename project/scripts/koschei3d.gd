extends Node3D
## Обёртка вокруг C++ KoscheiController3D (Кощей Бессмертный, 2.5D).
## Строит коллизии, хёртбокс, телеграфы, 3D-модель (koschei.glb) и запускает меч-кладенец.

const Util := preload("res://scripts/billboard_util.gd")
const VisualScript := preload("res://scripts/model_visual.gd")
const SwordScript := preload("res://scripts/sword3d.gd")

# koschei.glb: высота тела 1.431 ед. (y от -0.718) -> 3.0 м
const MODEL_PATH := "res://art/models/koschei.glb"
const MODEL_SCALE := 3.0 / 1.4313
const MODEL_FEET_OFFSET := 0.7178 * MODEL_SCALE

signal defeated

var core: CharacterBody3D = null
var visual: Node3D = null
var sword: Node3D = null

func _ready() -> void:
    if not ClassDB.class_exists("KoscheiController3D"):
        push_error("KoscheiController3D не найден. Пересобери GDExtension (scons) и проверь DLL.")
        return
    core = ClassDB.instantiate("KoscheiController3D") as CharacterBody3D
    if core == null:
        push_error("Не удалось создать KoscheiController3D.")
        return

    _build_core_scene(core)
    add_child(core)
    core.position = Vector3.ZERO
    core.connect("defeated", _on_core_defeated)

    # Меч-кладенец: отдельная сущность со своим ИИ, подчиняется Кощею.
    sword = SwordScript.new()
    sword.name = "Kladenets"
    sword.set("mesh_source", visual)
    add_child(sword)
    sword.call("place", core.global_position + Vector3(3.0, 1.7, 0.0))

func _build_core_scene(body: CharacterBody3D) -> void:
    body.collision_layer = 8
    body.collision_mask = 1

    body.add_child(Util.make_shadow(2.4, 0.6))

    visual = _make_model_visual()
    if visual != null:
        body.add_child(visual)
    else:
        # Запасной вариант без модели: тёмный столб (чтобы бой был проходим без импорта GLB).
        var capsule_mesh := CapsuleMesh.new()
        capsule_mesh.radius = 0.9
        capsule_mesh.height = 3.0
        var mat := StandardMaterial3D.new()
        mat.albedo_color = Color(0.55, 0.52, 0.6)
        capsule_mesh.material = mat
        var placeholder := MeshInstance3D.new()
        placeholder.mesh = capsule_mesh
        placeholder.position = Vector3(0, 1.5, 0)
        body.add_child(placeholder)

    var collision := CollisionShape3D.new()
    collision.name = "CollisionShape3D"
    var capsule := CapsuleShape3D.new()
    capsule.radius = 1.0
    capsule.height = 2.8
    collision.shape = capsule
    collision.position = Vector3(0, 1.4, 0)
    body.add_child(collision)

    # Хёртбокс: в него попадают AttackArea героя.
    var hurtbox := Area3D.new()
    hurtbox.name = "Hurtbox"
    hurtbox.collision_layer = 4
    hurtbox.collision_mask = 2
    body.add_child(hurtbox)
    var hurt_collision := CollisionShape3D.new()
    hurt_collision.name = "CollisionShape3D"
    var hurt_shape := CapsuleShape3D.new()
    hurt_shape.radius = 1.3
    hurt_shape.height = 3.2
    hurt_collision.shape = hurt_shape
    hurt_collision.position = Vector3(0, 1.6, 0)
    hurtbox.add_child(hurt_collision)

    # Телеграфы: круг (нова / приземление), прямоугольник когтей, полоса выпада. C++ ставит и масштабирует их сам.
    var telegraph := Util.make_disc(Color(0.75, 0.15, 0.55, 0.28), 0.05)
    telegraph.name = "Telegraph"
    telegraph.visible = false
    body.add_child(telegraph)
    body.add_child(_make_plane("MeleeTelegraph", Vector2(3.9, 3.4), Vector3(1.95, 0.0, 0.0), Color(0.9, 0.15, 0.1, 0.32)))
    body.add_child(_make_plane("LungeTelegraph", Vector2(1.0, 1.8), Vector3(0.5, 0.0, 0.0), Color(0.95, 0.2, 0.35, 0.30)))

func _make_plane(node_name: String, size: Vector2, center_offset: Vector3, color: Color) -> MeshInstance3D:
    var plane := PlaneMesh.new()
    plane.size = size
    plane.center_offset = center_offset
    var material := StandardMaterial3D.new()
    material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    material.cull_mode = BaseMaterial3D.CULL_DISABLED
    material.albedo_color = color
    plane.material = material
    var mi := MeshInstance3D.new()
    mi.name = node_name
    mi.mesh = plane
    mi.position = Vector3(0, 0.06, 0)
    mi.visible = false
    return mi

func _make_model_visual() -> Node3D:
    if not ResourceLoader.exists(MODEL_PATH):
        return null
    var scene := load(MODEL_PATH) as PackedScene
    if scene == null:
        return null
    var v := Node3D.new()
    v.name = "Visual"
    v.set_script(VisualScript)
    if not bool(v.call("setup", "koschei", scene, MODEL_SCALE, MODEL_FEET_OFFSET)):
        v.free()
        return null
    return v

func _on_core_defeated() -> void:
    # Ядро сейчас освободится (queue_free) — модель забираем себе, чтобы доиграть смерть.
    if visual != null:
        visual.call("detach_for_death", self)
    defeated.emit()

func set_arena_half(half: Vector2) -> void:
    if core != null and is_instance_valid(core):
        core.call("set_arena_half", half)

func get_health() -> float:
    if core == null or not is_instance_valid(core):
        return 0.0
    return float(core.call("get_health"))

func get_max_health() -> float:
    if core == null or not is_instance_valid(core):
        return 1.0
    return float(core.call("get_max_health"))

func get_phase() -> int:
    if core == null or not is_instance_valid(core):
        return 3
    return int(core.call("get_phase"))

func is_invulnerable() -> bool:
    if core == null or not is_instance_valid(core):
        return false
    return bool(core.call("is_invulnerable"))

func get_action() -> String:
    if core == null or not is_instance_valid(core):
        return "none"
    return String(core.call("get_action_name"))

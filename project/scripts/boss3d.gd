extends Node3D
## Обёртка вокруг C++ BossController3D (Леший в 2.5D).
## Спрайт без ели подменяется, когда Леший её бросает (сигнал spruce_changed).

const Util := preload("res://scripts/billboard_util.gd")
const SPRITE_FULL := "res://art/sprites/leshy_boss.png"
const SPRITE_NO_SPRUCE := "res://art/sprites/leshy_boss_nospruce.png"
const SPRITE_HEIGHT_M := 3.4

# 3D-модель Лешего (leshy.glb): высота 1.176 ед. (y от -0.593) -> 3.3 м
const VisualScript := preload("res://scripts/model_visual.gd")
const MODEL_PATH := "res://art/models/leshy.glb"
const MODEL_SCALE := 3.3 / 1.1758
const MODEL_FEET_OFFSET := 0.5929 * MODEL_SCALE

signal defeated

var core: CharacterBody3D = null
var sprite: Sprite3D = null
var tex_full: Texture2D = null
var tex_no_spruce: Texture2D = null
var visual: Node3D = null

func _ready() -> void:
    if not ClassDB.class_exists("BossController3D"):
        push_error("BossController3D не найден. Пересобери GDExtension (scons) и проверь DLL.")
        return

    core = ClassDB.instantiate("BossController3D") as CharacterBody3D
    if core == null:
        push_error("Не удалось создать BossController3D.")
        return

    _build_core_scene(core)
    add_child(core)
    core.position = Vector3.ZERO

    if core.has_signal("defeated"):
        core.connect("defeated", _on_core_defeated)
    if core.has_signal("spruce_changed"):
        core.connect("spruce_changed", _on_spruce_changed)

func _build_core_scene(body: CharacterBody3D) -> void:
    body.collision_layer = 8
    body.collision_mask = 1

    body.add_child(Util.make_shadow(2.2, 0.6))

    visual = _make_model_visual()
    if visual != null:
        body.add_child(visual)
    else:
        # Модель не найдена / не импортирована — запасной вариант: спрайт-билборд.
        sprite = Util.make_sprite(SPRITE_FULL, SPRITE_HEIGHT_M)
        tex_full = sprite.texture
        tex_no_spruce = Util.make_texture(SPRITE_NO_SPRUCE)
        body.add_child(sprite)

    var collision := CollisionShape3D.new()
    collision.name = "CollisionShape3D"
    var capsule := CapsuleShape3D.new()
    capsule.radius = 1.0
    capsule.height = 2.6
    collision.shape = capsule
    collision.position = Vector3(0, 1.3, 0)
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
    hurt_shape.radius = 1.15
    hurt_shape.height = 3.0
    hurt_collision.shape = hurt_shape
    hurt_collision.position = Vector3(0, 1.5, 0)
    hurtbox.add_child(hurt_collision)

    # Круг-телеграф (бросок ели; C++ сам ставит его и масштабирует).
    var telegraph := Util.make_disc(Color(0.85, 0.13, 0.05, 0.28), 0.05)
    telegraph.name = "Telegraph"
    telegraph.visible = false
    body.add_child(telegraph)

    # Телеграф удара рукой: прямоугольник вперёд от Лешего (C++ поворачивает его на героя).
    var plane := PlaneMesh.new()
    plane.size = Vector2(3.4, 3.0)
    plane.center_offset = Vector3(1.7, 0.0, 0.0)
    var material := StandardMaterial3D.new()
    material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    material.cull_mode = BaseMaterial3D.CULL_DISABLED
    material.albedo_color = Color(0.9, 0.15, 0.05, 0.32)
    plane.material = material
    var melee := MeshInstance3D.new()
    melee.name = "MeleeTelegraph"
    melee.mesh = plane
    melee.position = Vector3(0, 0.06, 0)
    melee.visible = false
    body.add_child(melee)

func _make_model_visual() -> Node3D:
    if not ResourceLoader.exists(MODEL_PATH):
        return null
    var scene := load(MODEL_PATH) as PackedScene
    if scene == null:
        return null
    var v := Node3D.new()
    v.name = "Visual"
    v.set_script(VisualScript)
    if not bool(v.call("setup", "boss", scene, MODEL_SCALE, MODEL_FEET_OFFSET)):
        v.free()
        return null
    return v

func _on_spruce_changed(held: bool) -> void:
    if visual != null:
        visual.call("set_spruce_visible", held)
    elif sprite != null:
        sprite.texture = tex_full if held else tex_no_spruce

## Копия меша ели для летящего снаряда (null, если работаем на спрайтах).
func make_spruce_visual() -> Node3D:
    if visual == null:
        return null
    return visual.call("make_spruce_clone") as Node3D

func _on_core_defeated() -> void:
    # Ядро сейчас освободится — забираем модель себе, чтобы доиграла анимацию смерти.
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

func get_spruce_state() -> String:
    if core == null or not is_instance_valid(core):
        return "held"
    return String(core.call("get_spruce_state_name"))

func get_action() -> String:
    if core == null or not is_instance_valid(core):
        return "none"
    return String(core.call("get_action_name"))

extends Node3D
## Обёртка вокруг C++ PlayerController3D (2.5D-версия героя).
## Как и в 2D: обычный Node3D, чтобы .tscn не зависел от GDExtension при парсинге.

const Util := preload("res://scripts/billboard_util.gd")
const Factory := preload("res://scripts/world_factory.gd")
const VisualScript := preload("res://scripts/model_visual.gd")
const SPRITE_PATH := "res://art/sprites/player_hero.png"
const SPRITE_HEIGHT_M := 1.8

# 3D-модель героя (hero.glb): высота модели 1.902 ед. (y от -0.951 до +0.951) -> 1.8 м
const MODEL_PATH := "res://art/models/hero.glb"
const MODEL_SCALE := 1.8 / 1.9022
const MODEL_FEET_OFFSET := 0.9512 * MODEL_SCALE

signal died

var core: CharacterBody3D = null

func _ready() -> void:
	if not ClassDB.class_exists("PlayerController3D"):
		push_error("PlayerController3D не найден. Пересобери GDExtension (scons) и проверь DLL.")
		return

	core = ClassDB.instantiate("PlayerController3D") as CharacterBody3D
	if core == null:
		push_error("Не удалось создать PlayerController3D.")
		return

	_build_core_scene(core)
	add_child(core)
	core.position = Vector3.ZERO

	if core.has_signal("died"):
		core.connect("died", func(): died.emit())
	if core.has_signal("throw_requested"):
		core.connect("throw_requested", _on_throw_requested)

## Умение Q — метание камня: C++ просит снаряд, визуал и объект создаём здесь.
func _on_throw_requested(origin: Vector3, direction: Vector3, damage: float) -> void:
	var stone := Factory.make_stone(origin, direction, damage)
	if stone != null:
		get_parent().add_child(stone)

func _build_core_scene(body: CharacterBody3D) -> void:
	# Слой героя (16) отличается от слоя стен (1): враги (маска 1) героя не "видят" и не двигаются от него.
	# От тела босса героя выталкивает сам босс (push_out), а не физика.
	body.collision_layer = 16
	body.collision_mask = 1

	body.add_child(Util.make_shadow(0.55))
	var visual := _make_model_visual()
	if visual != null:
		body.add_child(visual)
	else:
		# Модель не найдена / не импортирована — запасной вариант: спрайт-билборд.
		body.add_child(Util.make_sprite(SPRITE_PATH, SPRITE_HEIGHT_M))

	var body_collision := CollisionShape3D.new()
	body_collision.name = "CollisionShape3D"
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.3
	capsule.height = 1.7
	body_collision.shape = capsule
	body_collision.position = Vector3(0, 0.85, 0)
	body.add_child(body_collision)

	# --- Обычная атака: сфера перед героем + плоская вспышка на полу ---
	var attack_area := Area3D.new()
	attack_area.name = "AttackArea"
	attack_area.collision_layer = 2
	attack_area.collision_mask = 4
	body.add_child(attack_area)

	var attack_shape_node := CollisionShape3D.new()
	attack_shape_node.name = "CollisionShape3D"
	var attack_shape := SphereShape3D.new()
	attack_shape.radius = 0.8
	attack_shape_node.shape = attack_shape
	attack_shape_node.position = Vector3(0, 0.9, 0)
	attack_area.add_child(attack_shape_node)

	var attack_visual := Util.make_box(Vector3(1.3, 0.03, 0.8), Color(0.95, 0.60, 0.14, 0.75))
	attack_visual.name = "Visual"
	attack_visual.position = Vector3(0.15, 0.06, 0)
	attack_area.add_child(attack_visual)

func _make_model_visual() -> Node3D:
	if not ResourceLoader.exists(MODEL_PATH):
		return null
	var scene := load(MODEL_PATH) as PackedScene
	if scene == null:
		return null
	var visual := Node3D.new()
	visual.name = "Visual"
	visual.set_script(VisualScript)
	if not bool(visual.call("setup", "hero", scene, MODEL_SCALE, MODEL_FEET_OFFSET)):
		visual.free()
		return null
	return visual

func set_active(active: bool) -> void:
	if core == null or not is_instance_valid(core):
		return
	if not active:
		core.set_velocity(Vector3.ZERO)
	core.set_physics_process(active)
	core.set_process(active)

func teleport(world_position: Vector3) -> void:
	if core != null:
		core.global_position = world_position

func get_world_position() -> Vector3:
	return core.global_position if core != null else global_position

func get_health() -> float:
	return float(core.call("get_health")) if core != null else 0.0

func get_max_health() -> float:
	return float(core.call("get_max_health")) if core != null else 1.0

func get_dash_cooldown_left() -> float:
	return float(core.call("get_dash_cooldown_left")) if core != null else 0.0

func get_skill_cooldown_left() -> float:
	return float(core.call("get_skill_cooldown_left")) if core != null else 0.0

func is_dead() -> bool:
	return core != null and bool(core.call("is_dead"))

func get_stun_left() -> float:
	return float(core.call("get_stun_left")) if core != null else 0.0

func get_slow_left() -> float:
	return float(core.call("get_slow_left")) if core != null else 0.0

func get_weaken_left() -> float:
	return float(core.call("get_weaken_left")) if core != null else 0.0

func get_vulnerable_left() -> float:
	return float(core.call("get_vulnerable_left")) if core != null else 0.0

func get_silence_left() -> float:
	return float(core.call("get_silence_left")) if core != null else 0.0

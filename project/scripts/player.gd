extends Node2D
## Godot wrapper around the C++ PlayerController.
## The wrapper is deliberately a normal Node2D so the .tscn never depends
## on a custom GDExtension class during parsing.

var core: CharacterBody2D = null
var setup_ok := false

func _ready() -> void:
	if not ClassDB.class_exists("PlayerController"):
		push_error("PlayerController не найден. Проверь загрузку GDExtension и DLL.")
		return

	core = ClassDB.instantiate("PlayerController") as CharacterBody2D
	if core == null:
		push_error("Не удалось создать PlayerController.")
		return

	_build_core_scene(core)
	add_child(core)
	core.position = Vector2.ZERO
	setup_ok = true

	if core.has_signal("died"):
		core.connect("died", _on_core_died)

func _build_core_scene(controller: CharacterBody2D) -> void:
	controller.collision_layer = 1
	controller.collision_mask = 1

	var shadow := Polygon2D.new()
	shadow.name = "Shadow"
	shadow.polygon = PackedVector2Array([
		Vector2(-18, 12), Vector2(-10, 20), Vector2(10, 20),
		Vector2(18, 12), Vector2(10, 8), Vector2(-10, 8)
	])
	shadow.color = Color(0.01, 0.008, 0.006, 0.45)
	shadow.z_index = -1
	controller.add_child(shadow)

	var body := Polygon2D.new()
	body.name = "Body"
	body.polygon = PackedVector2Array([
		Vector2(-17, 8), Vector2(-19, -5), Vector2(-9, -23),
		Vector2(0, -28), Vector2(9, -23), Vector2(19, -5),
		Vector2(17, 8), Vector2(0, 17)
	])
	body.color = Color(0.70, 0.18, 0.10, 1)
	controller.add_child(body)

	var weapon := Polygon2D.new()
	weapon.name = "Weapon"
	weapon.polygon = PackedVector2Array([
		Vector2(0, -5), Vector2(48, -2), Vector2(51, 2), Vector2(2, 6)
	])
	weapon.color = Color(0.75, 0.65, 0.42, 1)
	controller.add_child(weapon)

	var body_collision := CollisionShape2D.new()
	body_collision.name = "CollisionShape2D"
	var capsule := CapsuleShape2D.new()
	capsule.radius = 17.0
	capsule.height = 40.0
	body_collision.shape = capsule
	controller.add_child(body_collision)

	var attack_area := Area2D.new()
	attack_area.name = "AttackArea"
	attack_area.collision_layer = 2
	attack_area.collision_mask = 4
	controller.add_child(attack_area)

	var attack_shape_node := CollisionShape2D.new()
	attack_shape_node.name = "CollisionShape2D"
	var attack_shape := CircleShape2D.new()
	attack_shape.radius = 48.0
	attack_shape_node.shape = attack_shape
	attack_area.add_child(attack_shape_node)

	var attack_visual := Polygon2D.new()
	attack_visual.name = "Polygon2D"
	attack_visual.polygon = PackedVector2Array([
		Vector2(0, -13), Vector2(35, -20), Vector2(72, 0),
		Vector2(35, 20), Vector2(0, 13)
	])
	attack_visual.color = Color(0.95, 0.60, 0.14, 0.86)
	attack_visual.visible = false
	attack_area.add_child(attack_visual)

	var skill_area := Area2D.new()
	skill_area.name = "SkillArea"
	skill_area.collision_layer = 2
	skill_area.collision_mask = 4
	controller.add_child(skill_area)

	var skill_shape_node := CollisionShape2D.new()
	skill_shape_node.name = "CollisionShape2D"
	var skill_shape := CircleShape2D.new()
	skill_shape.radius = 115.0
	skill_shape_node.shape = skill_shape
	skill_area.add_child(skill_shape_node)

	var skill_visual := Polygon2D.new()
	skill_visual.name = "Polygon2D"
	var points := PackedVector2Array()
	for i in range(40):
		var a := TAU * float(i) / 40.0
		points.append(Vector2(cos(a), sin(a)) * 114.0)
	skill_visual.polygon = points
	skill_visual.color = Color(0.90, 0.23, 0.08, 0.24)
	skill_visual.visible = false
	skill_area.add_child(skill_visual)

	var camera := Camera2D.new()
	camera.name = "Camera2D"
	camera.position_smoothing_enabled = true
	camera.position_smoothing_speed = 8.0
	controller.add_child(camera)

func _on_core_died() -> void:
	# Main.gd listens to the wrapper signal.
	died.emit()

signal died

func teleport(world_position: Vector2) -> void:
	if core != null:
		core.global_position = world_position

func get_world_position() -> Vector2:
	return core.global_position if core != null else global_position

func reset_run_stats() -> void:
	if core != null:
		core.call("reset_run_stats")

func increase_attack(value: float) -> void:
	if core != null:
		core.call("increase_attack", value)

func increase_max_health(value: float) -> void:
	if core != null:
		core.call("increase_max_health", value)

func reduce_dash_cooldown(value: float) -> void:
	if core != null:
		core.call("reduce_dash_cooldown", value)

func heal(value: float) -> void:
	if core != null:
		core.call("heal", value)

func get_health() -> float:
	return float(core.call("get_health")) if core != null else 0.0

func get_max_health() -> float:
	return float(core.call("get_max_health")) if core != null else 0.0

func get_attack_damage_multiplier() -> float:
	return float(core.call("get_attack_damage_multiplier")) if core != null else 1.0

func get_dash_cooldown() -> float:
	return float(core.call("get_dash_cooldown")) if core != null else 0.60

func get_dash_cooldown_left() -> float:
	return float(core.call("get_dash_cooldown_left")) if core != null else 0.0

func get_skill_cooldown_left() -> float:
	return float(core.call("get_skill_cooldown_left")) if core != null else 0.0

func is_dead() -> bool:
	return core != null and bool(core.call("is_dead"))

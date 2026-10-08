extends Node2D
## Normal Godot scene wrapper; the combat body is created dynamically as C++.

@export_enum("normal", "elite", "boss") var kind := "normal"

var core: CharacterBody2D = null

func _ready() -> void:
	if not ClassDB.class_exists("TrainingDummy"):
		push_error("TrainingDummy не найден. Проверь GDExtension.")
		return

	core = ClassDB.instantiate("TrainingDummy") as CharacterBody2D
	if core == null:
		push_error("Не удалось создать TrainingDummy.")
		return

	_build_core_scene(core)
	add_child(core)
	core.position = Vector2.ZERO

	core.tree_exited.connect(_on_core_tree_exited)
	call_deferred("_apply_kind")

func _build_core_scene(controller: CharacterBody2D) -> void:
	controller.collision_layer = 8
	controller.collision_mask = 1

	var shadow := Polygon2D.new()
	shadow.name = "Shadow"
	shadow.polygon = PackedVector2Array([
		Vector2(-18, 9), Vector2(-10, 16), Vector2(10, 16),
		Vector2(18, 9), Vector2(10, 6), Vector2(-10, 6)
	])
	shadow.color = Color(0.01, 0.008, 0.006, 0.40)
	shadow.z_index = -1
	controller.add_child(shadow)

	var body := Polygon2D.new()
	body.name = "Body"
	body.polygon = PackedVector2Array([
		Vector2(-22, 0), Vector2(-12, -19), Vector2(12, -19),
		Vector2(22, 0), Vector2(12, 19), Vector2(-12, 19)
	])
	body.color = Color(0.23, 0.58, 0.30, 1)
	controller.add_child(body)

	var collision := CollisionShape2D.new()
	collision.name = "CollisionShape2D"
	var circle := CircleShape2D.new()
	circle.radius = 22.0
	collision.shape = circle
	controller.add_child(collision)

	var hurtbox := Area2D.new()
	hurtbox.name = "Hurtbox"
	hurtbox.collision_layer = 4
	hurtbox.collision_mask = 2
	controller.add_child(hurtbox)

	var hurt_collision := CollisionShape2D.new()
	hurt_collision.name = "CollisionShape2D"
	var hurt_shape := CircleShape2D.new()
	hurt_shape.radius = 23.0
	hurt_collision.shape = hurt_shape
	hurtbox.add_child(hurt_collision)

	var hp_bar := ProgressBar.new()
	hp_bar.name = "HPBar"
	hp_bar.position = Vector2(-25, -38)
	hp_bar.size = Vector2(50, 8)
	hp_bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	hp_bar.max_value = 80.0
	hp_bar.value = 80.0
	hp_bar.show_percentage = false
	controller.add_child(hp_bar)

	var telegraph := Polygon2D.new()
	telegraph.name = "Telegraph"
	var points := PackedVector2Array()
	for i in range(40):
		var a := TAU * float(i) / 40.0
		points.append(Vector2(cos(a), sin(a)) * 180.0)
	telegraph.polygon = points
	telegraph.color = Color(0.85, 0.13, 0.05, 0.22)
	telegraph.visible = false
	telegraph.z_index = -2
	controller.add_child(telegraph)

func _apply_kind() -> void:
	if core == null:
		return
	if kind == "elite":
		core.set_elite(true)
	elif kind == "boss":
		core.set_boss(true)

func _on_core_tree_exited() -> void:
	if is_inside_tree():
		queue_free()

func set_elite(value: bool) -> void:
	kind = "elite" if value else "normal"
	if core != null:
		core.call("set_elite", value)

func set_boss(value: bool) -> void:
	kind = "boss" if value else "normal"
	if core != null:
		core.call("set_boss", value)

func get_health() -> float:
	return float(core.call("get_health")) if core != null else 0.0

func get_max_health() -> float:
	return float(core.call("get_max_health")) if core != null else 0.0

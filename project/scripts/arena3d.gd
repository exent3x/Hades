extends Node3D
## Босс-файт Лешего в 2.5D: герой и Леший — 3D-модели (или спрайты, если модели нет).
## В основной игре арену запускает main.gd (embedded = true) и ждёт сигнал fight_finished.
## Отдельный тест: в хабе нажми T, либо открой arena3d.tscn и нажми F6 (R — рестарт, ESC — выход).
##
## Босс (C++) только просит сигналом spawn_requested создать корни, птиц, грибы, снаряды,
## приспешников и сузить арену — здесь эти запросы превращаются в объекты.

const PlayerScript := preload("res://scripts/player3d.gd")
const BossScript := preload("res://scripts/boss3d.gd")
const KoscheiScript := preload("res://scripts/koschei3d.gd")
const Util := preload("res://scripts/billboard_util.gd")
const Factory := preload("res://scripts/world_factory.gd")

const MAIN_SCENE := "res://scenes/main/main.tscn"
const ARENA_SIZE := Vector2(17.0, 10.0)   # X, Z в метрах
const WALL_HEIGHT := 3.0
const CAMERA_OFFSET := Vector3(0.0, 11.0, 7.7)  # даёт угол ~55° вниз
const CAMERA_PITCH_DEG := -55.0
const SHRINK_SPEED_TIME := 1.5            # сек на полное сужение/расширение

## Бой закончен (после паузы на сообщение). Только в embedded-режиме.
signal fight_finished(victory: bool, hp_left: float)

## Какой босс на арене: "leshy" (лес) или "koschei" (Мёртвый град). main.gd задаёт его до add_child().
@export var boss_kind := "leshy"
## true — арену запустил main.gd: R/ESC отключены, итог уходит сигналом.
var embedded := false
## Билд героя из основного забега: max_hp, hp, attack_mult, dash_cd.
var run_stats: Dictionary = {}
var end_sent := false

var player = null  # player3d.gd
var boss = null    # boss3d.gd
var camera: Camera3D = null

var player_bar: ProgressBar
var boss_bar: ProgressBar
var boss_label: Label
var status_label: Label
var message_label: Label
var finished := false

# Сужение арены.
var shrink_scale_now := 1.0
var shrink_target := 1.0
var shrink_speed := 0.0
var shrink_hold_left := 0.0
var shrink_active := false
var brambles: Array = []

func _ready() -> void:
	_build_environment()
	_build_arena()
	if boss_kind == "koschei":
		_build_crypt()
	else:
		_build_forest()
	_spawn_actors()
	_apply_run_stats()
	_build_camera()
	_build_hud()

func _build_environment() -> void:
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.02, 0.015, 0.012)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.85, 0.85, 0.80)
	env.ambient_light_energy = 1.1
	if boss_kind == "koschei":
		# Чертог Кощея: холодный фиолетовый полумрак.
		env.background_color = Color(0.012, 0.008, 0.02)
		env.ambient_light_color = Color(0.70, 0.66, 0.95)
		env.ambient_light_energy = 1.0

	var world_env := WorldEnvironment.new()
	world_env.environment = env
	add_child(world_env)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-60, -30, 0)
	sun.light_energy = 1.0
	add_child(sun)

func _build_arena() -> void:
	# Пол — только визуальный (без коллайдера): движение идёт строго по плоскости.
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = ARENA_SIZE
	var floor_material := StandardMaterial3D.new()
	floor_material.albedo_color = Color(0.12, 0.075, 0.045)
	floor_material.roughness = 1.0
	floor_mesh.material = floor_material
	var floor_instance := MeshInstance3D.new()
	floor_instance.name = "Floor"
	floor_instance.mesh = floor_mesh
	add_child(floor_instance)

	# Круг ритуальной арены, как в 2D-комнате босса.
	var ring := Util.make_disc(Color(0.20, 0.10, 0.05, 0.55), 0.01)
	ring.scale = Vector3(4.7, 1.0, 4.7)
	add_child(ring)

	var half := ARENA_SIZE * 0.5
	var t := 0.5  # толщина стен
	_add_wall(Vector3(0, WALL_HEIGHT * 0.5, -half.y - t * 0.5), Vector3(ARENA_SIZE.x + t * 2.0, WALL_HEIGHT, t))
	_add_wall(Vector3(0, WALL_HEIGHT * 0.5, half.y + t * 0.5), Vector3(ARENA_SIZE.x + t * 2.0, WALL_HEIGHT, t))
	_add_wall(Vector3(-half.x - t * 0.5, WALL_HEIGHT * 0.5, 0), Vector3(t, WALL_HEIGHT, ARENA_SIZE.y))
	_add_wall(Vector3(half.x + t * 0.5, WALL_HEIGHT * 0.5, 0), Vector3(t, WALL_HEIGHT, ARENA_SIZE.y))

	# Колючие заросли: 4 полосы, закрывающие кольцо между полной и суженной ареной.
	for i in range(4):
		var bramble := Util.make_box(Vector3.ONE, Color(0.10, 0.20, 0.07, 0.92))
		bramble.visible = false
		add_child(bramble)
		brambles.append(bramble)

func _add_wall(pos: Vector3, size: Vector3) -> void:
	var wall := StaticBody3D.new()
	wall.collision_layer = 1
	wall.collision_mask = 0
	wall.position = pos

	var shape_node := CollisionShape3D.new()
	var shape := BoxShape3D.new()
	shape.size = size
	shape_node.shape = shape
	wall.add_child(shape_node)

	var visual := Util.make_box(size, Color(0.05, 0.035, 0.025, 1.0))
	wall.add_child(visual)
	add_child(wall)

## Тёмный ельник вокруг арены — это именно лесной босс-файт.
func _build_forest() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 7
	var trunk_mat := StandardMaterial3D.new()
	trunk_mat.albedo_color = Color(0.10, 0.06, 0.04)
	var leaf_mat := StandardMaterial3D.new()
	leaf_mat.albedo_color = Color(0.04, 0.12, 0.06)
	var half := ARENA_SIZE * 0.5
	for i in range(34):
		var x := 0.0
		var z := 0.0
		if rng.randf() < 0.5:
			x = rng.randf_range(-half.x - 6.0, half.x + 6.0)
			z = (half.y + rng.randf_range(1.6, 6.0)) * (1.0 if rng.randf() < 0.5 else -1.0)
		else:
			x = (half.x + rng.randf_range(1.6, 6.0)) * (1.0 if rng.randf() < 0.5 else -1.0)
			z = rng.randf_range(-half.y - 4.0, half.y + 4.0)
		var tree := Node3D.new()
		tree.position = Vector3(x, 0.0, z)
		var scale_k := rng.randf_range(0.8, 1.5)

		var trunk_mesh := CylinderMesh.new()
		trunk_mesh.top_radius = 0.22
		trunk_mesh.bottom_radius = 0.32
		trunk_mesh.height = 2.0
		trunk_mesh.material = trunk_mat
		var trunk := MeshInstance3D.new()
		trunk.mesh = trunk_mesh
		trunk.position = Vector3(0, 1.0, 0)
		tree.add_child(trunk)

		for j in range(3):
			var cone := CylinderMesh.new()
			cone.top_radius = 0.0
			cone.bottom_radius = 1.7 - j * 0.45
			cone.height = 2.2
			cone.material = leaf_mat
			var layer := MeshInstance3D.new()
			layer.mesh = cone
			layer.position = Vector3(0, 2.4 + j * 1.3, 0)
			tree.add_child(layer)
		tree.scale = Vector3.ONE * scale_k
		add_child(tree)

## Чертог Кощея: колонны и костяные факелы вокруг арены.
func _build_crypt() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 11
	var stone_mat := StandardMaterial3D.new()
	stone_mat.albedo_color = Color(0.13, 0.12, 0.17)
	var half := ARENA_SIZE * 0.5
	for i in range(22):
		var x := 0.0
		var z := 0.0
		if i % 2 == 0:
			x = lerpf(-half.x - 2.0, half.x + 2.0, float(i) / 22.0)
			z = (half.y + rng.randf_range(1.6, 4.5)) * (1.0 if (i / 2) % 2 == 0 else -1.0)
		else:
			x = (half.x + rng.randf_range(1.6, 4.5)) * (1.0 if (i / 2) % 2 == 0 else -1.0)
			z = lerpf(-half.y - 2.0, half.y + 2.0, float(i) / 22.0)
		var col_mesh := CylinderMesh.new()
		col_mesh.top_radius = 0.55
		col_mesh.bottom_radius = 0.7
		col_mesh.height = rng.randf_range(4.0, 7.0)
		col_mesh.material = stone_mat
		var column := MeshInstance3D.new()
		column.mesh = col_mesh
		column.position = Vector3(x, col_mesh.height * 0.5, z)
		add_child(column)
	# фиолетовые светильники по углам арены
	for corner in [Vector3(-half.x, 2.4, -half.y), Vector3(half.x, 2.4, -half.y), Vector3(-half.x, 2.4, half.y), Vector3(half.x, 2.4, half.y)]:
		var torch := OmniLight3D.new()
		torch.light_color = Color(0.62, 0.40, 1.0)
		torch.light_energy = 1.6
		torch.omni_range = 9.0
		torch.position = corner
		add_child(torch)

## Билд героя из основной игры переносится в 3D-бой.
func _apply_run_stats() -> void:
	if run_stats.is_empty() or player == null or player.core == null:
		return
	var hero_core = player.core
	var max_hp := float(run_stats.get("max_hp", 100.0))
	var cur_hp := float(run_stats.get("hp", max_hp))
	var attack_mult := float(run_stats.get("attack_mult", 1.0))
	var dash_cd := float(run_stats.get("dash_cd", 0.6))
	if max_hp > 100.0:
		hero_core.call("increase_max_health", max_hp - 100.0)
	hero_core.call("set_health", cur_hp)
	if attack_mult > 1.0:
		hero_core.call("increase_attack", attack_mult - 1.0)
	if dash_cd < 0.6:
		hero_core.call("reduce_dash_cooldown", 0.6 - dash_cd)

func _spawn_actors() -> void:
	player = PlayerScript.new()
	player.name = "Player3D"
	add_child(player)
	player.global_position = Vector3(0, 0, 3.0)
	player.died.connect(_on_player_died)

	boss = KoscheiScript.new() if boss_kind == "koschei" else BossScript.new()
	boss.name = "Koschei3D" if boss_kind == "koschei" else "Leshy3D"
	add_child(boss)
	boss.global_position = Vector3(0, 0, -3.0)
	boss.defeated.connect(_on_boss_defeated)
	if boss.core != null:
		boss.core.connect("spawn_requested", _on_boss_spawn_request)

func _build_camera() -> void:
	camera = Camera3D.new()
	camera.name = "Camera3D"
	camera.fov = 35.0
	camera.rotation_degrees = Vector3(CAMERA_PITCH_DEG, 0, 0)
	add_child(camera)
	camera.global_position = player.global_position + CAMERA_OFFSET
	camera.current = true

func _build_hud() -> void:
	var layer := CanvasLayer.new()
	add_child(layer)

	player_bar = ProgressBar.new()
	player_bar.position = Vector2(24, 24)
	player_bar.size = Vector2(260, 20)
	player_bar.show_percentage = false
	player_bar.max_value = 100.0
	player_bar.value = 100.0
	layer.add_child(player_bar)

	status_label = Label.new()
	status_label.position = Vector2(24, 50)
	layer.add_child(status_label)

	boss_label = Label.new()
	boss_label.position = Vector2(256, 20)
	boss_label.size = Vector2(640, 30)
	boss_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	boss_label.add_theme_font_size_override("font_size", 20)
	boss_label.add_theme_color_override("font_color", Color(0.90, 0.68, 0.45))
	layer.add_child(boss_label)

	boss_bar = ProgressBar.new()
	boss_bar.position = Vector2(256, 52)
	boss_bar.size = Vector2(640, 20)
	boss_bar.show_percentage = false
	layer.add_child(boss_bar)

	message_label = Label.new()
	message_label.position = Vector2(176, 250)
	message_label.size = Vector2(800, 90)
	message_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	message_label.add_theme_font_size_override("font_size", 34)
	layer.add_child(message_label)

	var hint := Label.new()
	hint.text = "WASD — движение  •  ЛКМ — атака  •  SPACE — дэш (прыжок через корни!)  •  Q — метнуть камень"
	if not embedded:
		hint.text += "  •  R — рестарт  •  ESC — выход"
	hint.position = Vector2(24, 616)
	hint.add_theme_color_override("font_color", Color(0.75, 0.7, 0.65))
	layer.add_child(hint)

func _process(delta: float) -> void:
	_update_shrink(delta)

	if camera != null and player != null:
		var desired: Vector3 = player.get_world_position() + CAMERA_OFFSET
		camera.global_position = camera.global_position.lerp(desired, 1.0 - exp(-8.0 * delta))

	if player != null:
		player_bar.max_value = player.get_max_health()
		player_bar.value = player.get_health()
		status_label.text = _player_status_text()

	if boss != null and not finished:
		boss_bar.max_value = boss.get_max_health()
		boss_bar.value = boss.get_health()
		if boss_kind == "koschei":
			boss_label.text = "КОЩЕЙ БЕССМЕРТНЫЙ   ФАЗА %d   •   %d / %d HP%s" % [
				boss.get_phase(), int(boss.get_health()), int(boss.get_max_health()),
				"   •   НЕУЯЗВИМ" if boss.is_invulnerable() else ""]
		else:
			boss_label.text = "ЛЕШИЙ   ФАЗА %d   •   %d / %d HP   •   Ель: %s" % [
				boss.get_phase(), int(boss.get_health()), int(boss.get_max_health()),
				_spruce_text(boss.get_spruce_state())]

func _spruce_text(state: String) -> String:
	match state:
		"held":
			return "в руке"
		"flying":
			return "летит"
		_:
			return "в земле — бей, пока он идёт за ней!"

func _player_status_text() -> String:
	var lines := "Дэш: %.1f   Камень (Q): %.1f" % [player.get_dash_cooldown_left(), player.get_skill_cooldown_left()]
	if player.get_stun_left() > 0.0:
		lines += "\nОПУТАН КОРНЯМИ  %.1f" % player.get_stun_left()
	if player.get_slow_left() > 0.0:
		lines += "\nЗамедление  %.1f" % player.get_slow_left()
	if player.get_weaken_left() > 0.0:
		lines += "\nОслабление (−урон)  %.1f" % player.get_weaken_left()
	if player.get_vulnerable_left() > 0.0:
		lines += "\nУЯЗВИМОСТЬ (+урон по тебе)  %.1f" % player.get_vulnerable_left()
	if player.get_silence_left() > 0.0:
		lines += "\nНемота (нельзя метать камень)  %.1f" % player.get_silence_left()
	return lines

# ===================================================================== запросы босса

func _on_boss_spawn_request(kind: String, pos: Vector3, data: Dictionary) -> void:
	if finished:
		return
	match kind:
		"root", "birds", "mushroom", "blast", "miasma":
			var hazard := Factory.make_hazard(kind, pos, data)
			if hazard != null:
				add_child(hazard)
		"minion":
			var minion := Factory.make_minion()
			add_child(minion)
			minion.global_position = Vector3(pos.x, 0.0, pos.z)
		"skeleton":
			var skeleton := Factory.make_minion("skeleton")
			add_child(skeleton)
			skeleton.global_position = Vector3(pos.x, 0.0, pos.z)
		"projectile":
			var spruce_visual: Node3D = null
			if String(data.get("kind", "")) == "spruce":
				spruce_visual = boss.make_spruce_visual()
			var proj := Factory.make_boss_projectile(data, boss.core, spruce_visual)
			if proj != null:
				add_child(proj)
		"shrink_arena":
			_start_shrink(float(data.get("duration", 9.0)), float(data.get("scale", 0.62)))

# ===================================================================== сужение арены

func _start_shrink(duration: float, scale_to: float) -> void:
	shrink_target = scale_to
	shrink_hold_left = duration
	shrink_active = true
	shrink_speed = (1.0 - scale_to) / SHRINK_SPEED_TIME
	_flash_message("ЧАЩА СМЫКАЕТСЯ!", 2.0)

func _update_shrink(delta: float) -> void:
	if shrink_active:
		shrink_scale_now = move_toward(shrink_scale_now, shrink_target, shrink_speed * delta)
		if is_equal_approx(shrink_scale_now, shrink_target):
			shrink_hold_left -= delta
			if shrink_hold_left <= 0.0:
				shrink_active = false
	else:
		shrink_scale_now = move_toward(shrink_scale_now, 1.0, maxf(shrink_speed, 0.1) * delta)

	var full := ARENA_SIZE * 0.5
	var half := full * shrink_scale_now

	if boss != null and boss.core != null:
		boss.set_arena_half(half)

	var shrunk := shrink_scale_now < 0.999
	for b in brambles:
		b.visible = shrunk
	if shrunk:
		_layout_brambles(full, half)
		_clamp_actors(half)

func _layout_brambles(full: Vector2, half: Vector2) -> void:
	var h := 0.7
	# верх / низ — на всю ширину, лево / право — между ними
	_place_bramble(brambles[0], Vector3(0, h * 0.5, -(full.y + half.y) * 0.5), Vector3(full.x * 2.0, h, full.y - half.y))
	_place_bramble(brambles[1], Vector3(0, h * 0.5, (full.y + half.y) * 0.5), Vector3(full.x * 2.0, h, full.y - half.y))
	_place_bramble(brambles[2], Vector3(-(full.x + half.x) * 0.5, h * 0.5, 0), Vector3(full.x - half.x, h, half.y * 2.0))
	_place_bramble(brambles[3], Vector3((full.x + half.x) * 0.5, h * 0.5, 0), Vector3(full.x - half.x, h, half.y * 2.0))

func _place_bramble(node: MeshInstance3D, pos: Vector3, size: Vector3) -> void:
	node.position = pos
	node.scale = size

func _clamp_actors(half: Vector2) -> void:
	if player != null and is_instance_valid(player):
		var pc = player.core
		if pc != null and is_instance_valid(pc):
			_clamp_node(pc, half)
	if boss != null and is_instance_valid(boss):
		var bc = boss.core
		if bc != null and is_instance_valid(bc):
			_clamp_node(bc, half)
	for m in get_tree().get_nodes_in_group("minions"):
		if m != null and is_instance_valid(m):
			_clamp_node(m, half)

func _clamp_node(n, half: Vector2) -> void:
	if n == null or not is_instance_valid(n):
		return
	if not (n is Node3D):
		return
	var margin := 0.45
	var p: Vector3 = n.global_position
	p.x = clampf(p.x, -half.x + margin, half.x - margin)
	p.z = clampf(p.z, -half.y + margin, half.y - margin)
	n.global_position = p

# ===================================================================== конец боя

func _flash_message(text: String, seconds: float) -> void:
	message_label.text = text
	get_tree().create_timer(seconds).timeout.connect(func():
		if not finished and message_label.text == text:
			message_label.text = "")

func _clear_threats() -> void:
	for group_name in ["minions", "hazards", "spruce_pickup"]:
		for n in get_tree().get_nodes_in_group(group_name):
			n.queue_free()

func _on_boss_defeated() -> void:
	finished = true
	boss_label.text = "КОЩЕЙ ПОВЕРЖЕН" if boss_kind == "koschei" else "ЛЕШИЙ ПОВЕРЖЕН"
	boss_bar.value = 0
	message_label.text = "ПОБЕДА" if embedded else "ПОБЕДА\nR — ещё раз"
	_clear_threats()
	_send_result(true, 3.0)

func _on_player_died() -> void:
	finished = true
	message_label.text = "ВЫ ПАЛИ" if embedded else "ВЫ ПАЛИ\nR — ещё раз"
	_send_result(false, 2.5)

## В основной игре: после паузы на сообщение отдаём итог main.gd.
func _send_result(victory: bool, delay: float) -> void:
	if not embedded or end_sent:
		return
	end_sent = true
	var hp_left := 0.0
	if victory and player != null and is_instance_valid(player):
		hp_left = player.get_health()
	get_tree().create_timer(delay).timeout.connect(func(): fight_finished.emit(victory, hp_left))

func _unhandled_input(event: InputEvent) -> void:
	if embedded:
		return
	if not event is InputEventKey or not event.pressed or event.echo:
		return
	if event.keycode == KEY_R:
		get_tree().reload_current_scene()
	elif event.keycode == KEY_ESCAPE:
		get_tree().change_scene_to_file(MAIN_SCENE)

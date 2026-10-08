extends Node2D

const HUB_SCENE := "res://scenes/world/hub.tscn"
const ROOM_1_SCENE := "res://scenes/world/rooms/forest_room_01.tscn"
const ROOM_2_SCENE := "res://scenes/world/rooms/forest_room_02.tscn"
const BOSS_SCENE := "res://scenes/world/rooms/leshy_boss_room.tscn"
const HUD_SCENE := "res://scenes/ui/hud.tscn"
const REWARD_SCENE := "res://scenes/ui/reward_overlay.tscn"
const PLAYER_SCENE := "res://scenes/player/player.tscn"
const SAVE_PATH := "user://russian_roguelite_save.cfg"

var player: Node = null
var current_room: Node = null
var hud: Control = null
var reward_overlay: Control = null
var state := "boot"
var floor_index := 0
var glory := 0
var permanent_hp_bonus := 0.0
var run_glory := 0
var next_room_path := ""

func _ready() -> void:
	_load_save()
	_create_player()
	_create_hud()
	call_deferred("_show_hub")

func _unhandled_input(event: InputEvent) -> void:
	if not event is InputEventKey or not event.pressed or event.echo:
		return

	if state == "hub" and (event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER):
		_start_run()
	elif state == "victory" and (event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER):
		_show_hub()
	elif state == "defeat" and (event.keycode == KEY_ENTER or event.keycode == KEY_KP_ENTER):
		_show_hub()
	elif state == "hub" and event.keycode == KEY_U:
		_buy_permanent_upgrade()

func _create_player() -> void:
	var scene := load(PLAYER_SCENE) as PackedScene
	if scene == null:
		push_error("Не удалось загрузить Player.tscn")
		return

	player = scene.instantiate()
	$World/Player.add_child(player)
	player.name = "Player"
	player.connect("died", _on_player_died)

func _create_hud() -> void:
	var scene := load(HUD_SCENE) as PackedScene
	if scene == null:
		push_error("Не удалось загрузить HUD.tscn")
		return
	hud = scene.instantiate()
	$UI.add_child(hud)
	hud.call("set_game", self)

	var reward_scene := load(REWARD_SCENE) as PackedScene
	if reward_scene == null:
		push_error("Не удалось загрузить reward_overlay.tscn")
		return
	reward_overlay = reward_scene.instantiate()
	$UI.add_child(reward_overlay)
	reward_overlay.call("set_game", self)
	reward_overlay.hide()

func _clear_room() -> void:
	if current_room != null and is_instance_valid(current_room):
		current_room.queue_free()
	current_room = null

func _show_hub() -> void:
	_clear_room()
	state = "hub"
	floor_index = 0
	run_glory = 0

	var scene := load(HUB_SCENE) as PackedScene
	if scene == null:
		push_error("Не удалось загрузить hub.tscn")
		return
	current_room = scene.instantiate()
	$World/Room.add_child(current_room)

	if player != null:
		player.call("reset_run_stats")
		if permanent_hp_bonus > 0.0:
			player.call("increase_max_health", permanent_hp_bonus)
		player.call("heal", float(player.call("get_max_health")))
		player.call("teleport", Vector2(0, 110))

	if reward_overlay != null:
		reward_overlay.hide()

	hud.call("set_mode", "hub", "ГРАНИЦА ЯВИ")
	hud.call("show_message", "ENTER — начать поход\nU — 50 Вечной славы → +10 стартового HP", 4.0)
	hud.call("set_glory", glory)

func _start_run() -> void:
	floor_index = 1
	run_glory = 0
	state = "combat"

	if player != null:
		player.call("reset_run_stats")
		if permanent_hp_bonus > 0.0:
			player.call("increase_max_health", permanent_hp_bonus)
		player.call("heal", float(player.call("get_max_health")))

	_enter_room(ROOM_1_SCENE, "ЧЁРНЫЙ ЛЕС — ОПУШКА")

func _enter_room(path: String, title: String) -> void:
	_clear_room()
	if reward_overlay != null:
		reward_overlay.hide()

	var scene := load(path) as PackedScene
	if scene == null:
		push_error("Не удалось загрузить комнату: " + path)
		return

	current_room = scene.instantiate()
	$World/Room.add_child(current_room)

	if current_room.has_method("setup"):
		current_room.call("setup", self, player)

	if player != null:
		player.call("teleport", current_room.call("get_player_start"))

	state = "combat"
	hud.call("set_mode", "combat", title)
	hud.call("show_message", "")

func on_combat_room_cleared(room_id: int) -> void:
	if room_id == 1:
		next_room_path = ROOM_2_SCENE
		_show_reward("НАГРАДА: ОПУШКА")
	elif room_id == 2:
		next_room_path = BOSS_SCENE
		_show_reward("НАГРАДА ПЕРЕД ЛОГОВОМ ЛЕШЕГО")

func _show_reward(title: String) -> void:
	state = "reward"
	hud.call("set_mode", "reward", title)
	hud.call("show_message", "Выбери одно улучшение", 5.0)
	reward_overlay.call("show_reward", title)

func apply_reward(choice: int) -> void:
	if player == null:
		return

	match choice:
		1:
			player.call("increase_attack", 0.20)
		2:
			player.call("increase_max_health", 20.0)
			player.call("heal", 20.0)
		3:
			player.call("reduce_dash_cooldown", 0.08)

	if reward_overlay != null:
		reward_overlay.hide()

	if next_room_path == BOSS_SCENE:
		_enter_boss()
	else:
		floor_index = 2
		_enter_room(next_room_path, "ЧЁРНЫЙ ЛЕС — СТАРЫЙ ТРАК")

func _enter_boss() -> void:
	_clear_room()
	var scene := load(BOSS_SCENE) as PackedScene
	if scene == null:
		push_error("Не удалось загрузить boss room")
		return

	current_room = scene.instantiate()
	$World/Room.add_child(current_room)

	if current_room.has_method("setup"):
		current_room.call("setup", self, player)

	player.call("teleport", current_room.call("get_player_start"))
	state = "boss"
	hud.call("set_mode", "boss", "ЛОГОВО ЛЕШЕГО")
	hud.call("show_message", "Леший ждёт в центре леса.\nКрасный круг — телеграф удара.", 4.0)

func on_boss_defeated() -> void:
	state = "victory"
	run_glory = 100 + floor_index * 25
	glory += run_glory
	_save()
	hud.call("set_mode", "victory", "ПОБЕДА")
	hud.call("show_message", "Леший повержен.\nПолучено Вечной славы: %d\nENTER — вернуться в убежище" % run_glory, 999.0)

func _on_player_died() -> void:
	if state == "defeat" or state == "victory" or state == "hub":
		return

	state = "defeat"
	if reward_overlay != null:
		reward_overlay.hide()
	hud.call("set_mode", "defeat", "ПОХОД ЗАКОНЧЕН")
	hud.call("show_message", "Ты погиб в Чёрном лесу.\nВременные улучшения потеряны.\nENTER — вернуться в убежище", 999.0)

func _buy_permanent_upgrade() -> void:
	if state != "hub":
		return
	if glory < 50:
		hud.call("show_message", "Нужно 50 Вечной славы. Сейчас: %d" % glory, 3.0)
		return

	glory -= 50
	permanent_hp_bonus += 10.0
	_save()
	_show_hub()

func get_player() -> Node:
	return player

func get_run_glory() -> int:
	return run_glory

func get_glory() -> int:
	return glory

func get_permanent_hp_bonus() -> float:
	return permanent_hp_bonus

func _load_save() -> void:
	var cfg := ConfigFile.new()
	if cfg.load(SAVE_PATH) != OK:
		return
	glory = int(cfg.get_value("meta", "glory", 0))
	permanent_hp_bonus = float(cfg.get_value("meta", "hp_bonus", 0.0))

func _save() -> void:
	var cfg := ConfigFile.new()
	cfg.set_value("meta", "glory", glory)
	cfg.set_value("meta", "hp_bonus", permanent_hp_bonus)
	cfg.save(SAVE_PATH)

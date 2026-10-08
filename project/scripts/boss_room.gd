extends Node2D

var game: Node = null
var player: Node = null
var boss_scene: PackedScene = null
var finished := false
var started := false

func setup(game_ref: Node, player_ref: Node) -> void:
	game = game_ref
	player = player_ref
	boss_scene = load("res://scenes/enemies/leshy_boss.tscn") as PackedScene
	call_deferred("_spawn_boss")

func get_player_start() -> Vector2:
	return $PlayerStart.position

func _spawn_boss() -> void:
	started = true
	if boss_scene == null:
		$Status.text = "Ошибка: сцена Лешего не загружена"
		return

	var boss := boss_scene.instantiate()
	$Boss.add_child(boss)
	boss.global_position = $BossSpawn.global_position
	$Status.text = "ЛЕШИЙ — 900 HP"

func _process(_delta: float) -> void:
	if not started or finished:
		return

	if $Boss.get_child_count() == 0:
		finished = true
		$Status.text = "ЛЕШИЙ ПОВЕРЖЕН"
		if game != null:
			game.call("on_boss_defeated")
		return

	var boss := $Boss.get_child(0)
	if boss != null and boss.has_method("get_health"):
		$BossBar.max_value = boss.get_max_health()
		$BossBar.value = boss.get_health()

		var ratio : float = boss.get_health() / boss.get_max_health()
		var phase := 1 if ratio > 0.66 else (2 if ratio > 0.33 else 3)
		$Status.text = "ЛЕШИЙ   ФАЗА %d   •   %d / %d HP" % [
			phase, int(boss.get_health()), int(boss.get_max_health())
		]

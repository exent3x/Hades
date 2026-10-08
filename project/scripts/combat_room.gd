extends Node2D

@export var room_id: int = 1
@export var wave_count: int = 2
@export var enemies_per_wave: Array[int] = [4, 5]
@export var elite_on_last_wave := true

var game: Node = null
var player: Node = null
var wave := 0
var waiting := false
var cleared := false
var spawn_timer := 0.0
var started := false
var enemy_scene: PackedScene = null

func setup(game_ref: Node, player_ref: Node) -> void:
    game = game_ref
    player = player_ref
    enemy_scene = load("res://scenes/enemies/training_dummy.tscn") as PackedScene
    call_deferred("_start_room")

func get_player_start() -> Vector2:
    return $PlayerStart.position

func _start_room() -> void:
    started = true
    wave = 1
    $DoorClosed.show()
    $DoorOpen.hide()
    _set_door_collision(true)
    _spawn_wave(wave)

func _process(delta: float) -> void:
    if not started or cleared:
        return

    if waiting:
        spawn_timer -= delta
        if spawn_timer <= 0.0:
            wave += 1
            waiting = false
            _spawn_wave(wave)
        return

    if $Enemies.get_child_count() == 0:
        if wave < wave_count:
            waiting = true
            spawn_timer = 1.1
            $Status.text = "ВОЛНА %d ОКОНЧЕНА  •  следующая через %.1f" % [wave, spawn_timer]
        else:
            cleared = true
            $DoorClosed.hide()
            $DoorOpen.show()
            _set_door_collision(false)
            $Status.text = "АРЕНА ОЧИЩЕНА  •  Награда получена"
            if game != null:
                game.call("on_combat_room_cleared", room_id)

func _spawn_wave(wave_number: int) -> void:
    if enemy_scene == null:
        $Status.text = "Ошибка: не загружена сцена врага"
        return

    var count := enemies_per_wave[min(wave_number - 1, enemies_per_wave.size() - 1)]
    var points := $EnemySpawns.get_children()

    for i in range(count):
        var enemy := enemy_scene.instantiate()
        $Enemies.add_child(enemy)
        enemy.global_position = points[i % points.size()].global_position
        enemy.call("set_elite", elite_on_last_wave and wave_number == wave_count and i == count - 1)

    $Status.text = "ВОЛНА %d / %d   •   ВРАГОВ: %d" % [wave_number, wave_count, count]

func _set_door_collision(value: bool) -> void:
    if has_node("DoorBlock/CollisionShape2D"):
        $DoorBlock/CollisionShape2D.set_deferred("disabled", not value)

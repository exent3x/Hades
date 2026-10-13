extends Node3D
## Боевая комната в 3D: волны врагов на арене с препятствиями. Заменяет 2D-комнаты маршрута.
## Запускается из main.gd (embedded): после победы шлёт room_cleared(hp), после гибели — player_died.
##
## Состав волн зависит от глубины маршрута (params.depth 0..9), типа узла (params.elite) и локации (params.theme):
##   налётчики и прыгуны с самого начала, громилы — со 2-го этапа, стрелки — с 3-го; элиты на последней волне
##   (в «элитной» комнате — с первой волны, по две штуки).

const PlayerScript := preload("res://scripts/player3d.gd")
const Util := preload("res://scripts/billboard_util.gd")
const Factory := preload("res://scripts/world_factory.gd")

signal room_cleared(hp_left: float)
signal player_died

const CAMERA_OFFSET := Vector3(0.0, 11.0, 7.7)
const CAMERA_PITCH_DEG := -55.0
const WALL_HEIGHT := 3.0
const COST := {"raider": 1.0, "jumper": 1.0, "archer": 1.3, "brute": 2.4}

var embedded := true
## Билд героя из забега: max_hp, hp, attack_mult, dash_cd.
var run_stats: Dictionary = {}
## depth (0..9), elite (bool), theme ("forest" / "dead_city"), title.
var params: Dictionary = {"depth": 0, "elite": false, "theme": "forest", "title": ""}

var rng := RandomNumberGenerator.new()
var arena_size := Vector2(16.0, 11.0)
var obstacles: Array = []     # [Vector3(x, 0, z)], радиус хранится в w отдельно
var obstacle_radius: Array = []

var player = null
var camera: Camera3D = null
var enemies_root: Node3D = null

var wave := 0
var wave_total := 2
var timer := 1.4
var phase := "intro"          # intro / fighting / between / cleared / over
var end_sent := false

var player_bar: ProgressBar
var status_label: Label
var debuff_label: Label
var message_label: Label
var hud_layer: CanvasLayer

func _ready() -> void:
    rng.seed = 1000 + int(params.get("depth", 0)) * 17 + (1 if bool(params.get("elite", false)) else 0)
    rng.randomize()
    var depth := int(params.get("depth", 0))
    wave_total = 3 if (depth >= 5 or bool(params.get("elite", false))) else 2
    arena_size = Vector2(rng.randf_range(15.0, 18.0), rng.randf_range(10.0, 12.0))

    _build_environment()
    _build_arena()
    _build_obstacles()
    _build_decor()
    enemies_root = Node3D.new()
    enemies_root.name = "Enemies"
    add_child(enemies_root)
    _spawn_player()
    _build_camera()
    _build_hud()

# ===================================================================== окружение

func _theme_city() -> bool:
    return String(params.get("theme", "forest")) == "dead_city"

func _build_environment() -> void:
    var env := Environment.new()
    env.background_mode = Environment.BG_COLOR
    env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    if _theme_city():
        env.background_color = Color(0.015, 0.01, 0.025)
        env.ambient_light_color = Color(0.72, 0.68, 0.95)
        env.ambient_light_energy = 1.0
    else:
        env.background_color = Color(0.02, 0.015, 0.012)
        env.ambient_light_color = Color(0.85, 0.85, 0.80)
        env.ambient_light_energy = 1.1
    var world_env := WorldEnvironment.new()
    world_env.environment = env
    add_child(world_env)
    var sun := DirectionalLight3D.new()
    sun.rotation_degrees = Vector3(-60, -30, 0)
    sun.light_energy = 1.0
    add_child(sun)

func _build_arena() -> void:
    var floor_mesh := PlaneMesh.new()
    floor_mesh.size = arena_size
    var floor_mat := StandardMaterial3D.new()
    floor_mat.albedo_color = Color(0.10, 0.09, 0.13) if _theme_city() else Color(0.12, 0.075, 0.045)
    floor_mat.roughness = 1.0
    floor_mesh.material = floor_mat
    var floor_instance := MeshInstance3D.new()
    floor_instance.mesh = floor_mesh
    add_child(floor_instance)

    var half := arena_size * 0.5
    var t := 0.5
    var wall_color := Color(0.06, 0.05, 0.09) if _theme_city() else Color(0.05, 0.035, 0.025)
    _add_wall(Vector3(0, WALL_HEIGHT * 0.5, -half.y - t * 0.5), Vector3(arena_size.x + t * 2.0, WALL_HEIGHT, t), wall_color)
    _add_wall(Vector3(0, WALL_HEIGHT * 0.5, half.y + t * 0.5), Vector3(arena_size.x + t * 2.0, WALL_HEIGHT, t), wall_color)
    _add_wall(Vector3(-half.x - t * 0.5, WALL_HEIGHT * 0.5, 0), Vector3(t, WALL_HEIGHT, arena_size.y), wall_color)
    _add_wall(Vector3(half.x + t * 0.5, WALL_HEIGHT * 0.5, 0), Vector3(t, WALL_HEIGHT, arena_size.y), wall_color)

func _add_wall(pos: Vector3, size: Vector3, color: Color) -> void:
    var wall := StaticBody3D.new()
    wall.collision_layer = 1
    wall.collision_mask = 0
    wall.position = pos
    var shape_node := CollisionShape3D.new()
    var shape := BoxShape3D.new()
    shape.size = size
    shape_node.shape = shape
    wall.add_child(shape_node)
    wall.add_child(Util.make_box(size, color))
    add_child(wall)

## Препятствия: пни и валуны в лесу, обломки колонн в городе. Герой и враги упираются в них (укрытия от стрелков).
func _build_obstacles() -> void:
    var half := arena_size * 0.5
    var count := rng.randi_range(4, 7)
    var tries := 0
    while obstacles.size() < count and tries < 80:
        tries += 1
        var r := rng.randf_range(0.6, 1.0)
        var p := Vector3(rng.randf_range(-half.x + 2.5, half.x - 2.5), 0.0, rng.randf_range(-half.y + 2.0, half.y - 2.0))
        if absf(p.x) < 2.2 and p.z > half.y - 4.5:
            continue                      # зона старта героя
        var ok := true
        for i in range(obstacles.size()):
            if p.distance_to(obstacles[i]) < r + float(obstacle_radius[i]) + 2.4:
                ok = false
        if not ok:
            continue
        obstacles.append(p)
        obstacle_radius.append(r)

        var body := StaticBody3D.new()
        body.collision_layer = 1
        body.collision_mask = 0
        body.position = p
        var shape_node := CollisionShape3D.new()
        var cyl := CylinderShape3D.new()
        cyl.radius = r
        cyl.height = 2.0
        shape_node.shape = cyl
        shape_node.position = Vector3(0, 1.0, 0)
        body.add_child(shape_node)

        var mesh := CylinderMesh.new()
        mesh.top_radius = r * 0.85
        mesh.bottom_radius = r
        mesh.height = rng.randf_range(1.2, 2.2)
        var mat := StandardMaterial3D.new()
        mat.albedo_color = Color(0.22, 0.20, 0.27) if _theme_city() else Color(0.20, 0.13, 0.08)
        mesh.material = mat
        var mi := MeshInstance3D.new()
        mi.mesh = mesh
        mi.position = Vector3(0, mesh.height * 0.5, 0)
        body.add_child(mi)
        add_child(body)

## Декор за стенами: ельник или колонны.
func _build_decor() -> void:
    var half := arena_size * 0.5
    var trunk_mat := StandardMaterial3D.new()
    trunk_mat.albedo_color = Color(0.10, 0.06, 0.04)
    var leaf_mat := StandardMaterial3D.new()
    leaf_mat.albedo_color = Color(0.04, 0.12, 0.06)
    var stone_mat := StandardMaterial3D.new()
    stone_mat.albedo_color = Color(0.13, 0.12, 0.17)
    for i in range(30):
        var side := rng.randi() % 4
        var x := rng.randf_range(-half.x - 5.0, half.x + 5.0)
        var z := rng.randf_range(-half.y - 4.0, half.y + 4.0)
        match side:
            0: z = -half.y - rng.randf_range(1.6, 6.0)
            1: z = half.y + rng.randf_range(1.6, 6.0)
            2: x = -half.x - rng.randf_range(1.6, 6.0)
            _: x = half.x + rng.randf_range(1.6, 6.0)
        if _theme_city():
            var col_mesh := CylinderMesh.new()
            col_mesh.top_radius = 0.55
            col_mesh.bottom_radius = 0.7
            col_mesh.height = rng.randf_range(4.0, 7.0)
            col_mesh.material = stone_mat
            var column := MeshInstance3D.new()
            column.mesh = col_mesh
            column.position = Vector3(x, col_mesh.height * 0.5, z)
            add_child(column)
        else:
            var tree := Node3D.new()
            tree.position = Vector3(x, 0.0, z)
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
            tree.scale = Vector3.ONE * rng.randf_range(0.8, 1.5)
            add_child(tree)
    if _theme_city():
        for corner in [Vector3(-half.x, 2.4, -half.y), Vector3(half.x, 2.4, -half.y), Vector3(-half.x, 2.4, half.y), Vector3(half.x, 2.4, half.y)]:
            var torch := OmniLight3D.new()
            torch.light_color = Color(0.62, 0.40, 1.0)
            torch.light_energy = 1.4
            torch.omni_range = 9.0
            torch.position = corner
            add_child(torch)

# ===================================================================== герой и камера

func _spawn_player() -> void:
    player = PlayerScript.new()
    player.name = "Player3D"
    add_child(player)
    player.global_position = Vector3(0, 0, arena_size.y * 0.5 - 2.2)
    player.died.connect(_on_player_died)
    _apply_run_stats()

## Билд героя из основной игры переносится в 3D-бой.
func _apply_run_stats() -> void:
    if run_stats.is_empty() or player == null or player.core == null:
        return
    var hero = player.core
    var max_hp := float(run_stats.get("max_hp", 100.0))
    var cur_hp := float(run_stats.get("hp", max_hp))
    var attack_mult := float(run_stats.get("attack_mult", 1.0))
    var dash_cd := float(run_stats.get("dash_cd", 0.6))
    if max_hp > 100.0:
        hero.call("increase_max_health", max_hp - 100.0)
    hero.call("set_health", cur_hp)
    if attack_mult > 1.0:
        hero.call("increase_attack", attack_mult - 1.0)
    if dash_cd < 0.6:
        hero.call("reduce_dash_cooldown", 0.6 - dash_cd)

func _build_camera() -> void:
    camera = Camera3D.new()
    camera.fov = 35.0
    camera.rotation_degrees = Vector3(CAMERA_PITCH_DEG, 0, 0)
    add_child(camera)
    camera.global_position = player.global_position + CAMERA_OFFSET
    camera.current = true

func _build_hud() -> void:
    hud_layer = CanvasLayer.new()
    add_child(hud_layer)

    player_bar = ProgressBar.new()
    player_bar.position = Vector2(24, 24)
    player_bar.size = Vector2(260, 20)
    player_bar.show_percentage = false
    hud_layer.add_child(player_bar)

    debuff_label = Label.new()
    debuff_label.position = Vector2(24, 50)
    hud_layer.add_child(debuff_label)

    status_label = Label.new()
    status_label.position = Vector2(256, 22)
    status_label.size = Vector2(640, 30)
    status_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    status_label.add_theme_font_size_override("font_size", 20)
    status_label.add_theme_color_override("font_color", Color(0.90, 0.68, 0.45))
    hud_layer.add_child(status_label)

    message_label = Label.new()
    message_label.position = Vector2(176, 250)
    message_label.size = Vector2(800, 90)
    message_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
    message_label.add_theme_font_size_override("font_size", 34)
    hud_layer.add_child(message_label)

    var hint := Label.new()
    hint.text = "WASD — движение  •  ЛКМ — атака  •  SPACE — дэш  •  Q — метнуть камень"
    hint.position = Vector2(24, 616)
    hint.add_theme_color_override("font_color", Color(0.75, 0.7, 0.65))
    hud_layer.add_child(hint)

# ===================================================================== волны

## Состав волны: бюджет растёт с глубиной и номером волны; роли открываются по мере прохождения.
func _compose_wave(index: int, total: int) -> Array:
    var depth := int(params.get("depth", 0))
    var elite_room := bool(params.get("elite", false))
    var budget := 3.5 + depth * 0.9 + index * 1.4 + (2.5 if elite_room else 0.0)

    var pool: Array = ["raider", "jumper"]
    if depth >= 1:
        pool.append("brute")
    if depth >= 2:
        pool.append("archer")
        pool.append("archer")
    if depth >= 4:
        pool.append("brute")
        pool.append("jumper")

    var list: Array = []
    var archers := 0
    var guard := 0
    while budget > 0.9 and list.size() < 9 and guard < 60:
        guard += 1
        var role: String = pool[rng.randi() % pool.size()]
        if role == "archer" and archers >= 3:
            continue
        if float(COST[role]) > budget + 0.6:
            role = "raider"
        list.append({"role": role, "elite": false})
        budget -= float(COST[role])
        if role == "archer":
            archers += 1

    # Элиты: последняя волна обычной комнаты (с 3-го этапа), в «элитной» комнате — с первой, по две.
    var elite_count := 0
    var last := index == total - 1
    if elite_room:
        elite_count = 1 if index == 0 else 2
    elif last and depth >= 2:
        elite_count = 2 if depth >= 6 else 1
    var kinds: Array = ["brute", "raider", "jumper", "archer"]
    kinds.shuffle()
    for i in range(elite_count):
        list.append({"role": kinds[i % kinds.size()], "elite": true})
    return list

func _spawn_point(role: String) -> Vector3:
    var half := arena_size * 0.5
    var pp: Vector3 = player.core.global_position if player != null and player.core != null else Vector3(0, 0, half.y)
    var best := Vector3(0, 0, -half.y + 1.5)
    var best_d := 0.0
    for i in range(24):
        var p := Vector3(rng.randf_range(-half.x + 1.4, half.x - 1.4), 0.0, rng.randf_range(-half.y + 1.4, half.y - 1.4))
        var d := p.distance_to(pp)
        if d < 7.0:
            continue
        var blocked := false
        for j in range(obstacles.size()):
            if p.distance_to(obstacles[j]) < float(obstacle_radius[j]) + 1.0:
                blocked = true
        if blocked:
            continue
        if role == "archer" and d < 9.0 and i < 18:
            continue                          # стрелки появляются подальше
        return p
    return best

func _start_wave() -> void:
    wave += 1
    phase = "fighting"
    var depth := int(params.get("depth", 0))
    var hp_mult := 1.0 + depth * 0.06
    var dmg_mult := 1.0 + depth * 0.04
    if _theme_city():
        hp_mult *= 1.15
        dmg_mult *= 1.1
    var half := arena_size * 0.5
    for entry in _compose_wave(wave - 1, wave_total):
        var enemy := Factory.make_enemy(String(entry["role"]), bool(entry["elite"]), hp_mult, dmg_mult, half)
        enemies_root.add_child(enemy)
        enemy.global_position = _spawn_point(String(entry["role"]))
        enemy.spawn_requested.connect(_on_enemy_spawn_request)

func _on_enemy_spawn_request(kind: String, pos: Vector3, data: Dictionary) -> void:
    if phase == "over":
        return
    match kind:
        "blast", "miasma":
            var hazard := Factory.make_hazard(kind, pos, data)
            if hazard != null:
                add_child(hazard)
        "projectile":
            var proj := Factory.make_boss_projectile(data, null)
            if proj != null:
                add_child(proj)

# ===================================================================== цикл

func _process(delta: float) -> void:
    if camera != null and player != null and player.core != null:
        camera.global_position = camera.global_position.lerp(player.core.global_position + CAMERA_OFFSET, 1.0 - exp(-8.0 * delta))
    if player != null:
        player_bar.max_value = player.get_max_health()
        player_bar.value = player.get_health()
        debuff_label.text = _debuff_text()

    var alive := get_tree().get_nodes_in_group("room_enemies").size()
    match phase:
        "intro":
            status_label.text = "ГОТОВЬСЯ…"
            timer -= delta
            if timer <= 0.0:
                _start_wave()
        "fighting":
            status_label.text = "ВОЛНА %d / %d   •   ВРАГОВ: %d" % [wave, wave_total, alive]
            if alive == 0:
                if wave < wave_total:
                    phase = "between"
                    timer = 1.3
                else:
                    phase = "cleared"
                    timer = 1.4
                    message_label.text = "АРЕНА ОЧИЩЕНА"
        "between":
            status_label.text = "ВОЛНА %d ОКОНЧЕНА" % wave
            timer -= delta
            if timer <= 0.0:
                _start_wave()
        "cleared":
            status_label.text = "АРЕНА ОЧИЩЕНА"
            timer -= delta
            if timer <= 0.0 and not end_sent:
                end_sent = true
                message_label.text = ""
                room_cleared.emit(player.get_health() if player != null else 1.0)

func _debuff_text() -> String:
    var lines := "Дэш: %.1f   Камень (Q): %.1f" % [player.get_dash_cooldown_left(), player.get_skill_cooldown_left()]
    if player.get_stun_left() > 0.0:
        lines += "\nОПУТАН  %.1f" % player.get_stun_left()
    if player.get_slow_left() > 0.0:
        lines += "\nЗамедление  %.1f" % player.get_slow_left()
    if player.get_weaken_left() > 0.0:
        lines += "\nОслабление (−урон)  %.1f" % player.get_weaken_left()
    if player.get_vulnerable_left() > 0.0:
        lines += "\nУЯЗВИМОСТЬ  %.1f" % player.get_vulnerable_left()
    if player.get_silence_left() > 0.0:
        lines += "\nНемота  %.1f" % player.get_silence_left()
    return lines

func _on_player_died() -> void:
    if phase == "over":
        return
    phase = "over"
    message_label.text = "ВЫ ПАЛИ"
    get_tree().create_timer(2.2).timeout.connect(func(): player_died.emit())

## Бой окончен: замораживаем комнату (она остаётся фоном под выбором награды) и прячем её HUD.
func freeze() -> void:
    process_mode = Node.PROCESS_MODE_DISABLED
    if hud_layer != null:
        hud_layer.visible = false

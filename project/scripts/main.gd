extends Node2D

const HUB_SCENE := "res://scenes/world/hub.tscn"
const MAP_SCENE := "res://scenes/world/world_map.tscn"
const ROOM_1_SCENE := "res://scenes/world/rooms/forest_room_01.tscn"
const ROOM_2_SCENE := "res://scenes/world/rooms/forest_room_02.tscn"
## Локация 2 «Мёртвый град»: пока копия комнат леса (city_room_*) — маршрут к Кощею. Потом можно заменить на свои.
const CITY_ROOM_1_SCENE := "res://scenes/world/rooms/city_room_01.tscn"
const CITY_ROOM_2_SCENE := "res://scenes/world/rooms/city_room_02.tscn"
const ARENA_KOSCHEI_SCENE := "res://scenes/test3d/arena_koschei.tscn"
## Боевые комнаты маршрута в 3D (волны врагов). Если классов GDExtension нет — работают старые 2D-комнаты.
const ROOM_3D_SCENE := "res://scenes/test3d/combat_room3d.tscn"
## Выбранная на карте локация: "forest" (Леший) или "dead_city" (Кощей).
var current_region := "forest"
const BOSS_SCENE := "res://scenes/world/rooms/leshy_boss_room.tscn"
## 2.5D-версия босс-файта (3D-модели/билборды). Если GDExtension-классов нет — используется 2D-комната выше.
const ARENA_3D_SCENE := "res://scenes/test3d/arena3d.tscn"
var arena_3d: Node = null
const HUD_SCENE := "res://scenes/ui/hud.tscn"
const REWARD_SCENE := "res://scenes/ui/reward_overlay.tscn"
const PLAYER_SCENE := "res://scenes/player/player.tscn"
const SAVE_PATH := "user://russian_roguelite_save.cfg"
const ROOMS_BEFORE_BOSS := 10

var player: Node = null
var current_room: Node = null
var world_map: Control = null
var hud: Control = null
var reward_overlay: Control = null
var state := "boot"
var floor_index := 0
var route_depth := 0
var route_path: Array = []
var current_node_title := ""
var current_node_type := "combat"
var last_map_result := ""
var glory := 0
var permanent_hp_bonus := 0.0
var run_glory := 0

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
    elif state == "hub" and event.keycode == KEY_T:
        # Тестовая 2.5D-арена (Леший). Основная 2D-игра не затрагивается.
        get_tree().change_scene_to_file("res://scenes/test3d/arena3d.tscn")
    elif state == "hub" and event.keycode == KEY_K:
        # Тестовая 2.5D-арена (Кощей Бессмертный).
        get_tree().change_scene_to_file(ARENA_KOSCHEI_SCENE)

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
    _free_arena_3d()
    if current_room != null and is_instance_valid(current_room):
        current_room.queue_free()
    current_room = null

func _remove_map() -> void:
    if world_map != null and is_instance_valid(world_map):
        var map_parent := world_map.get_parent()
        if map_parent != null:
            map_parent.remove_child(world_map)
        world_map.queue_free()
    world_map = null

func _show_hub() -> void:
    _free_arena_3d()
    current_region = "forest"
    _remove_map()
    _clear_room()
    state = "hub"
    floor_index = 0
    route_depth = 0
    route_path.clear()
    current_node_title = ""
    current_node_type = "combat"
    last_map_result = ""
    run_glory = 0

    var scene := load(HUB_SCENE) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить hub.tscn")
        return
    current_room = scene.instantiate()
    $World/Room.add_child(current_room)

    if player != null:
        player.show()
        if player.has_method("set_active"):
            player.call("set_active", true)
        player.call("reset_run_stats")
        if permanent_hp_bonus > 0.0:
            player.call("increase_max_health", permanent_hp_bonus)
        player.call("heal", float(player.call("get_max_health")))
        player.call("teleport", Vector2(0, 110))

    if reward_overlay != null:
        reward_overlay.hide()
    if hud != null:
        hud.show()
        hud.call("set_mode", "hub", "ГРАНИЦА ЯВИ")
        hud.call("show_message", "ENTER — начать поход и открыть карту\nU — 50 Вечной славы → +10 стартового HP\nT — тест Лешего  •  K — тест Кощея", 4.0)
        hud.call("set_glory", glory)

func _start_run() -> void:
    route_depth = 0
    route_path.clear()
    floor_index = 0
    run_glory = 0
    last_map_result = ""

    if player != null:
        player.call("reset_run_stats")
        if permanent_hp_bonus > 0.0:
            player.call("increase_max_health", permanent_hp_bonus)
        player.call("heal", float(player.call("get_max_health")))

    _show_world_map("regions")

func _show_world_map(mode: String, message: String = "") -> void:
    _free_arena_3d()
    _remove_map()
    state = "map_regions" if mode == "regions" else "route_map"
    last_map_result = message

    if player != null:
        player.hide()
        if player.has_method("set_active"):
            player.call("set_active", false)
    if hud != null:
        hud.hide()
    if reward_overlay != null:
        reward_overlay.hide()

    var scene := load(MAP_SCENE) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить карту похода: " + MAP_SCENE)
        return

    world_map = scene.instantiate() as Control
    $UI.add_child(world_map)
    world_map.connect("region_selected", _on_map_region_selected)
    world_map.connect("route_node_selected", _on_route_node_selected)
    world_map.connect("return_to_regions", _on_map_return_to_regions)
    world_map.connect("back_requested", _on_map_back_requested)

    if mode == "regions":
        world_map.call_deferred("show_regions")
    else:
        world_map.call_deferred("show_forest_route", route_depth, route_path.duplicate(), message, current_region)

func _on_map_region_selected(region_id: String) -> void:
    if (region_id != "forest" and region_id != "dead_city") or world_map == null:
        return
    current_region = region_id
    state = "route_map"
    world_map.call("show_forest_route", route_depth, route_path.duplicate(), last_map_result, current_region)

func _on_map_return_to_regions() -> void:
    if world_map == null:
        return
    state = "map_regions"
    world_map.call("show_regions")

func _on_map_back_requested() -> void:
    # Кнопка «Бросить поход» намеренно возвращает в хаб и сбрасывает временный билд.
    _show_hub()

func _on_route_node_selected(depth: int, option_index: int, node_type: String, title: String) -> void:
    if state != "route_map" or depth != route_depth:
        return

    _remove_map()
    current_node_title = title
    current_node_type = node_type

    if depth == ROOMS_BEFORE_BOSS:
        _enter_boss()
        return

    route_path.append(option_index)
    floor_index = route_depth + 1

    match node_type:
        "healing":
            _resolve_noncombat_node("healing", title)
        "treasure":
            _resolve_noncombat_node("treasure", title)
        "shrine":
            _resolve_noncombat_node("shrine", title)
        "event":
            _resolve_noncombat_node("event", title)
        "elite":
            _enter_room(CITY_ROOM_2_SCENE if current_region == "dead_city" else ROOM_2_SCENE, title + " — ЭЛИТА")
        _:
            var scene_path := ROOM_1_SCENE if route_depth % 2 == 0 else ROOM_2_SCENE
            if current_region == "dead_city":
                scene_path = CITY_ROOM_1_SCENE if route_depth % 2 == 0 else CITY_ROOM_2_SCENE
            _enter_room(scene_path, title)

func _resolve_noncombat_node(node_type: String, title: String) -> void:
    _clear_room()
    var result := ""
    var max_hp := float(player.call("get_max_health")) if player != null else 100.0

    match node_type:
        "healing":
            var restored := max_hp * 0.35
            if player != null:
                player.call("heal", restored)
            result = "%s: восстановлено %d HP." % [title, int(restored)]
        "treasure":
            if player != null:
                player.call("increase_attack", 0.12)
            run_glory += 25
            result = "%s: оружие усилено на 12%%. При победе получишь +25 Вечной славы." % title
        "shrine":
            if player != null:
                player.call("increase_max_health", 10.0)
                player.call("heal", 10.0)
            result = "%s: максимум здоровья увеличен на 10, ты восстановил 10 HP." % title
        "event":
            match randi_range(0, 2):
                0:
                    if player != null:
                        player.call("heal", max_hp * 0.20)
                    result = "%s: путник поделился лечебным настоем (+20%% HP)." % title
                1:
                    if player != null:
                        player.call("increase_attack", 0.08)
                    result = "%s: знак на коре подсказал слабость врагов (+8%% урона)." % title
                _:
                    run_glory += 20
                    result = "%s: ты нашёл старую монету. При победе получишь +20 Вечной славы." % title
        _:
            result = "%s: путь продолжается." % title

    route_depth += 1
    floor_index = route_depth
    _show_world_map("forest", result)

func _enter_room(path: String, title: String) -> void:
    _clear_room()
    _remove_map()
    if reward_overlay != null:
        reward_overlay.hide()

    if _can_use_rooms_3d():
        _enter_room_3d(title)
        return

    var scene := load(path) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить комнату: " + path)
        return

    current_room = scene.instantiate()
    $World/Room.add_child(current_room)

    if current_room.has_method("setup"):
        current_room.call("setup", self, player)

    if player != null:
        player.show()
        if player.has_method("set_active"):
            player.call("set_active", true)
        player.call("teleport", current_room.call("get_player_start"))

    state = "combat"
    if hud != null:
        hud.show()
        hud.call("set_mode", "combat", title)
        hud.call("show_message", "")

func _can_use_rooms_3d() -> bool:
    return ClassDB.class_exists("EnemyUnit3D") \
            and ClassDB.class_exists("PlayerController3D") \
            and ResourceLoader.exists(ROOM_3D_SCENE)

## Боевая комната в 3D: 2D-мир прячем, поверх запускаем комнату и ждём её сигналы.
func _enter_room_3d(title: String) -> void:
    var scene := load(ROOM_3D_SCENE) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить combat_room3d.tscn")
        return

    arena_3d = scene.instantiate()
    arena_3d.set("params", {
        "depth": route_depth,
        "elite": current_node_type == "elite",
        "theme": current_region,
        "title": title,
    })
    # Билд героя из забега переносится в 3D-бой (HP, урон, дэш).
    var stats := {}
    if player != null:
        stats["max_hp"] = float(player.call("get_max_health"))
        stats["hp"] = float(player.call("get_health"))
        stats["attack_mult"] = float(player.call("get_attack_damage_multiplier"))
        stats["dash_cd"] = float(player.call("get_dash_cooldown"))
    arena_3d.set("run_stats", stats)
    arena_3d.connect("room_cleared", _on_room_3d_cleared)
    arena_3d.connect("player_died", _on_player_died)

    $World.hide()
    if player != null:
        player.hide()
        if player.has_method("set_active"):
            player.call("set_active", false)
    if hud != null:
        hud.hide()

    state = "combat"
    add_child(arena_3d)

func _on_room_3d_cleared(hp_left: float) -> void:
    if state != "combat":
        return
    # Полученный в бою урон переносится обратно в забег (HP между комнатами не восстанавливается).
    if player != null:
        var lost := float(player.call("get_health")) - hp_left
        var core = player.get("core")
        if lost > 0.5 and core != null:
            core.call("take_damage", lost)
    if arena_3d != null and is_instance_valid(arena_3d):
        arena_3d.call("freeze")      # комната остаётся фоном под выбором награды
    on_combat_room_cleared(0)

func on_combat_room_cleared(_room_id: int) -> void:
    if state != "combat":
        return
    _show_reward("НАГРАДА: %s" % current_node_title)

func _show_reward(title: String) -> void:
    state = "reward"
    if player != null and player.has_method("set_active"):
        player.call("set_active", false)
    if hud != null:
        hud.show()
        hud.call("set_mode", "reward", title)
        hud.call("show_message", "Выбери одно улучшение", 5.0)
    if reward_overlay != null:
        reward_overlay.call("show_reward", title)

func apply_reward(choice: int) -> void:
    if player == null or state != "reward":
        return

    match choice:
        1:
            player.call("increase_attack", 0.20)
            last_map_result = "Награда: урон оружия увеличен на 20%."
        2:
            player.call("increase_max_health", 20.0)
            player.call("heal", 20.0)
            last_map_result = "Награда: +20 к максимуму здоровья и восстановлено 20 HP."
        3:
            player.call("reduce_dash_cooldown", 0.08)
            last_map_result = "Награда: перезарядка дэша сокращена."

    if reward_overlay != null:
        reward_overlay.hide()

    route_depth += 1
    floor_index = route_depth
    _show_world_map("forest", last_map_result)

func _enter_boss() -> void:
    _clear_room()
    _remove_map()
    if reward_overlay != null:
        reward_overlay.hide()

    if _can_use_boss_3d():
        _enter_boss_3d()
        return

    var scene := load(BOSS_SCENE) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить boss room")
        return

    current_room = scene.instantiate()
    $World/Room.add_child(current_room)

    if current_room.has_method("setup"):
        current_room.call("setup", self, player)

    if player != null:
        player.show()
        if player.has_method("set_active"):
            player.call("set_active", true)
        player.call("teleport", current_room.call("get_player_start"))

    state = "boss"
    if hud != null:
        hud.show()
        hud.call("set_mode", "boss", "ЛОГОВО ЛЕШЕГО")
        hud.call("show_message", "Леший ждёт в центре леса.\nКрасная область — телеграф удара.", 4.0)

func _can_use_boss_3d() -> bool:
    if current_region == "dead_city":
        return ClassDB.class_exists("KoscheiController3D") \
                and ClassDB.class_exists("PlayerController3D") \
                and ResourceLoader.exists(ARENA_KOSCHEI_SCENE)
    return ClassDB.class_exists("BossController3D") \
            and ClassDB.class_exists("PlayerController3D") \
            and ResourceLoader.exists(ARENA_3D_SCENE)

## Босс-файт в 2.5D: 2D-мир прячем, поверх запускаем арену и ждём её итог.
func _enter_boss_3d() -> void:
    var arena_path := ARENA_KOSCHEI_SCENE if current_region == "dead_city" else ARENA_3D_SCENE
    var scene := load(arena_path) as PackedScene
    if scene == null:
        push_error("Не удалось загрузить арену: " + arena_path)
        return

    arena_3d = scene.instantiate()
    arena_3d.set("embedded", true)
    # Билд героя из забега переносится в 3D-бой.
    var stats := {}
    if player != null:
        stats["max_hp"] = float(player.call("get_max_health"))
        stats["hp"] = float(player.call("get_health"))
        stats["attack_mult"] = float(player.call("get_attack_damage_multiplier"))
        stats["dash_cd"] = float(player.call("get_dash_cooldown"))
    arena_3d.set("run_stats", stats)
    arena_3d.connect("fight_finished", _on_arena_3d_finished)

    $World.hide()
    if player != null:
        player.hide()
        if player.has_method("set_active"):
            player.call("set_active", false)
    if hud != null:
        hud.hide()

    state = "boss"
    add_child(arena_3d)

func _on_arena_3d_finished(victory: bool, _hp_left: float) -> void:
    _free_arena_3d()
    if victory:
        on_boss_defeated()
    else:
        _on_player_died()

func _free_arena_3d() -> void:
    if arena_3d != null and is_instance_valid(arena_3d):
        arena_3d.queue_free()
    arena_3d = null
    $World.show()
    if hud != null:
        hud.show()

func on_boss_defeated() -> void:
    if state == "victory":
        return
    state = "victory"
    if player != null and player.has_method("set_active"):
        player.call("set_active", false)
    run_glory = 100 + route_depth * 25 + run_glory
    glory += run_glory
    _save()
    if hud != null:
        hud.show()
        hud.call("set_mode", "victory", "ПОБЕДА")
        var boss_name := "Кощей Бессмертный повержен." if current_region == "dead_city" else "Леший повержен."
        hud.call("show_message", boss_name + "\nПолучено Вечной славы: %d\nENTER — вернуться в убежище" % run_glory, 999.0)

func _on_player_died() -> void:
    if state in ["defeat", "victory", "hub", "map_regions", "route_map"]:
        return

    state = "defeat"
    if player != null and player.has_method("set_active"):
        player.call("set_active", false)
    if reward_overlay != null:
        reward_overlay.hide()
    if hud != null:
        hud.show()
        hud.call("set_mode", "defeat", "ПОХОД ЗАКОНЧЕН")
        var place := "в Мёртвом граде" if current_region == "dead_city" else "в Чёрном лесу"
        hud.call("show_message", "Ты погиб " + place + ".\nВременные улучшения потеряны.\nENTER — вернуться в убежище", 999.0)

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

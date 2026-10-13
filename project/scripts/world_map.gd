extends Control
## Карта похода. Вся навигация реализована обычными Godot Control-узлами,
## поэтому сцена карты не зависит от регистрации C++ классов.

signal region_selected(region_id: String)
signal route_node_selected(depth: int, option_index: int, node_type: String, title: String)
signal return_to_regions
signal back_requested

const REGIONS: Array[Dictionary] = [
    {"id": "forest", "title": "ЧЁРНЫЙ ЛЕС", "subtitle": "Корни, забытые капища и Леший", "unlocked": true},
    {"id": "marsh", "title": "ТОПИ МАРЫ", "subtitle": "Затонувшие деревни и болотные духи", "unlocked": false},
    {"id": "dead_city", "title": "МЁРТВЫЙ ГРАД", "subtitle": "Пустые палаты и Кощей Бессмертный", "unlocked": true},
    {"id": "north", "title": "СЕВЕРНЫЙ ПРЕДЕЛ", "subtitle": "Мороз, курганы и древние великаны", "unlocked": false},
    {"id": "ash_steppe", "title": "ПЕПЕЛЬНЫЕ СТЕПИ", "subtitle": "Кочевники, пепел и огненные духи", "unlocked": false},
    {"id": "nav", "title": "ПОДЗЕМЬЯ НАВИ", "subtitle": "Реки мёртвых и забытые имена", "unlocked": false},
    {"id": "viy", "title": "ЧЕРТОГИ ВИЯ", "subtitle": "Последняя дорога за грань", "unlocked": false}
]

# Десять этапов маршрута до босса. Игрок выбирает один из доступных узлов
# текущего этапа; тип узла влияет на происходящее после выбора.
const FOREST_STAGES: Array = [
    [
        {"type": "combat", "title": "Заросший брод", "short": "БОЙ"},
        {"type": "shrine", "title": "Каменный алтарь", "short": "АЛТАРЬ"},
        {"type": "treasure", "title": "Сундук под корнями", "short": "ТАЙНИК"}
    ],
    [
        {"type": "combat", "title": "Тропа волков", "short": "БОЙ"},
        {"type": "event", "title": "Странник у костра", "short": "СОБЫТИЕ"},
        {"type": "elite", "title": "Лесные дозорные", "short": "ЭЛИТА"}
    ],
    [
        {"type": "combat", "title": "Поляна костей", "short": "БОЙ"},
        {"type": "healing", "title": "Родник Яви", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Заброшенный стан", "short": "БОЙ"},
        {"type": "treasure", "title": "Старая кладовая", "short": "ТАЙНИК"},
        {"type": "event", "title": "Шёпот в коре", "short": "СОБЫТИЕ"}
    ],
    [
        {"type": "combat", "title": "Расколотая тропа", "short": "БОЙ"},
        {"type": "elite", "title": "Корневой страж", "short": "ЭЛИТА"}
    ],
    [
        {"type": "combat", "title": "Круг идолов", "short": "БОЙ"},
        {"type": "shrine", "title": "Забытое капище", "short": "АЛТАРЬ"},
        {"type": "healing", "title": "Лесной источник", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Охотничья застава", "short": "БОЙ"},
        {"type": "treasure", "title": "Гнездо домового", "short": "ТАЙНИК"}
    ],
    [
        {"type": "combat", "title": "Гиблый овраг", "short": "БОЙ"},
        {"type": "elite", "title": "Дикий витязь", "short": "ЭЛИТА"},
        {"type": "healing", "title": "Источник перед чащей", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Последний привал", "short": "БОЙ"},
        {"type": "event", "title": "Вещая ворона", "short": "СОБЫТИЕ"}
    ],
    [
        {"type": "combat", "title": "Древний курган", "short": "БОЙ"},
        {"type": "elite", "title": "Дубовый страж", "short": "ЭЛИТА"},
        {"type": "healing", "title": "Последний источник", "short": "ЛЕЧЕНИЕ"}
    ]
]

const DEAD_CITY_STAGES: Array = [
    [
        {"type": "combat", "title": "Разрушенные ворота", "short": "БОЙ"},
        {"type": "shrine", "title": "Костяной алтарь", "short": "АЛТАРЬ"},
        {"type": "treasure", "title": "Склеп с сокровищами", "short": "ТАЙНИК"}
    ],
    [
        {"type": "combat", "title": "Улица теней", "short": "БОЙ"},
        {"type": "event", "title": "Призрачный купец", "short": "СОБЫТИЕ"},
        {"type": "elite", "title": "Стража мёртвого града", "short": "ЭЛИТА"}
    ],
    [
        {"type": "combat", "title": "Площадь костей", "short": "БОЙ"},
        {"type": "healing", "title": "Колодец живой воды", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Брошенные казармы", "short": "БОЙ"},
        {"type": "treasure", "title": "Ледник палат", "short": "ТАЙНИК"},
        {"type": "event", "title": "Шёпот за стеной", "short": "СОБЫТИЕ"}
    ],
    [
        {"type": "combat", "title": "Расколотый мост", "short": "БОЙ"},
        {"type": "elite", "title": "Каменный страж", "short": "ЭЛИТА"}
    ],
    [
        {"type": "combat", "title": "Зал истуканов", "short": "БОЙ"},
        {"type": "shrine", "title": "Забытая часовня", "short": "АЛТАРЬ"},
        {"type": "healing", "title": "Тихий источник", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Княжеская застава", "short": "БОЙ"},
        {"type": "treasure", "title": "Тайник казначея", "short": "ТАЙНИК"}
    ],
    [
        {"type": "combat", "title": "Гиблый ров", "short": "БОЙ"},
        {"type": "elite", "title": "Мёртвый витязь", "short": "ЭЛИТА"},
        {"type": "healing", "title": "Источник перед палатами", "short": "ЛЕЧЕНИЕ"}
    ],
    [
        {"type": "combat", "title": "Последний привал у стен", "short": "БОЙ"},
        {"type": "event", "title": "Вещий ворон", "short": "СОБЫТИЕ"}
    ],
    [
        {"type": "combat", "title": "Княжеский курган", "short": "БОЙ"},
        {"type": "elite", "title": "Страж чертогов", "short": "ЭЛИТА"},
        {"type": "healing", "title": "Последний колодец", "short": "ЛЕЧЕНИЕ"}
    ]
]

const BOSS_DEPTH := 10
const NODE_SIZE := Vector2(78.0, 54.0)

const REGION_INFO := {
    "forest": {"title": "ЧЁРНЫЙ ЛЕС", "boss": "ЛЕШИЙ", "boss_node": "Логово Лешего",
            "open": "ПОСЛЕ ДЕСЯТОЙ КОМНАТЫ ОТКРОЕТСЯ ЛЕШИЙ", "done": "ПУТЬ ЗАВЕРШЁН  •  ВЫБЕРИ ЛОГОВО ЛЕШЕГО"},
    "dead_city": {"title": "МЁРТВЫЙ ГРАД", "boss": "КОЩЕЙ", "boss_node": "Чертоги Кощея",
            "open": "ПОСЛЕ ДЕСЯТОЙ КОМНАТЫ ОТКРОЕТСЯ КОЩЕЙ БЕССМЕРТНЫЙ", "done": "ПУТЬ ЗАВЕРШЁН  •  ВЫБЕРИ ЧЕРТОГИ КОЩЕЯ"},
}

var current_region := "forest"
var view_mode := "regions"
var current_depth := 0
var selected_path: Array = []
var status_message := ""

func _ready() -> void:
    set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    show_regions()

func show_regions() -> void:
    view_mode = "regions"
    status_message = ""
    _clear_children()
    queue_redraw()

    var view_size := get_viewport_rect().size
    _add_label("КАРТА ЯВИ", Vector2(35, 22), Vector2(view_size.x - 70, 42), 30, Color(0.89, 0.77, 0.56), HORIZONTAL_ALIGNMENT_CENTER)
    _add_label("Семь дорог ведут к разным бедам. Сейчас открыта только одна.", Vector2(50, 65), Vector2(view_size.x - 100, 28), 16, Color(0.69, 0.65, 0.56), HORIZONTAL_ALIGNMENT_CENTER)

    var card_w := minf(438.0, (view_size.x - 130.0) * 0.5)
    var card_h := 88.0
    var left_x := (view_size.x - card_w * 2.0 - 34.0) * 0.5
    var right_x := left_x + card_w + 34.0
    var row_y: Array[float] = [112.0, 218.0, 324.0]

    for i in range(6):
        var region: Dictionary = REGIONS[i]
        var column_x := left_x if i % 2 == 0 else right_x
        var row := int(i / 2)
        var region_id := str(region["id"])
        var unlocked := bool(region["unlocked"])
        var button := _make_button(
            "%s\n%s" % [region["title"], region["subtitle"] if unlocked else "ЗАПЕРТО — область пока недоступна"],
            Vector2(column_x, row_y[row]), Vector2(card_w, card_h),
            Color(0.17, 0.22, 0.15) if unlocked else Color(0.095, 0.09, 0.085),
            Color(0.25, 0.32, 0.20) if unlocked else Color(0.095, 0.09, 0.085),
            unlocked
        )
        button.add_theme_font_size_override("font_size", 16)
        button.tooltip_text = str(region["subtitle"])
        if unlocked:
            button.pressed.connect(_on_region_button_pressed.bind(region_id))

    var last_region: Dictionary = REGIONS[6]
    var final_w := card_w
    var final_x := (view_size.x - final_w) * 0.5
    var final_button := _make_button(
        "%s\nЗАПЕРТО — область пока недоступна" % last_region["title"],
        Vector2(final_x, 430.0), Vector2(final_w, card_h),
        Color(0.095, 0.09, 0.085), Color(0.095, 0.09, 0.085), false
    )
    final_button.add_theme_font_size_override("font_size", 16)
    _add_label("Выбери открытую дорогу мышью.\nДругие направления появятся по мере развития игры.", Vector2(45, 540), Vector2(view_size.x - 90, 56), 14, Color(0.60, 0.57, 0.50), HORIZONTAL_ALIGNMENT_CENTER)
    var back := _make_button("В УБЕЖИЩЕ", Vector2(view_size.x - 174, view_size.y - 48), Vector2(145, 30), Color(0.13, 0.12, 0.11), Color(0.22, 0.18, 0.14), true)
    back.add_theme_font_size_override("font_size", 12)
    back.pressed.connect(_on_back_pressed)

func show_forest_route(depth: int, path: Array, message: String = "", region_id: String = "forest") -> void:
    current_region = region_id if REGION_INFO.has(region_id) else "forest"
    view_mode = "forest"
    current_depth = clampi(depth, 0, BOSS_DEPTH)
    selected_path = path.duplicate()
    status_message = message
    _clear_children()
    queue_redraw()

    var view_size := get_viewport_rect().size
    _add_label(REGION_INFO[current_region]["title"], Vector2(28, 17), Vector2(view_size.x - 56, 36), 27, Color(0.89, 0.76, 0.54), HORIZONTAL_ALIGNMENT_CENTER)
    _add_label("ВЫБЕРИ ПУТЬ К ЛЕШЕМУ  •  10 ЭТАПОВ ДО БОССА", Vector2(28, 52), Vector2(view_size.x - 56, 25), 14, Color(0.70, 0.66, 0.56), HORIZONTAL_ALIGNMENT_CENTER)

    var back_text := "← К РЕГИОНАМ" if current_depth == 0 and selected_path.is_empty() else "БРОСИТЬ ПОХОД"
    var back := _make_button(back_text, Vector2(22, 18), Vector2(150, 32), Color(0.12, 0.11, 0.09), Color(0.23, 0.18, 0.12), true)
    back.add_theme_font_size_override("font_size", 12)
    if current_depth == 0 and selected_path.is_empty():
        back.pressed.connect(_on_return_to_regions_pressed)
    else:
        back.pressed.connect(_on_back_pressed)

    # Column labels and node buttons. Only the current stage is interactive.
    for depth_index in range(BOSS_DEPTH + 1):
        var nodes: Array = _nodes_for_depth(depth_index)
        for option_index in range(nodes.size()):
            var node_data: Dictionary = nodes[option_index]
            var center := _node_center(depth_index, option_index)
            var is_current := depth_index == current_depth
            var is_past := depth_index < current_depth
            var was_selected := is_past and depth_index < selected_path.size() and int(selected_path[depth_index]) == option_index
            var is_boss := depth_index == BOSS_DEPTH
            var label_text := "БОСС\n%s" % REGION_INFO[current_region]["boss"] if is_boss else "%02d\n%s" % [depth_index + 1, node_data["short"]]
            var tint := _node_color(str(node_data.get("type", "combat")))
            var enabled := is_current
            if was_selected:
                tint = Color(0.24, 0.37, 0.21)
            elif not enabled:
                tint = Color(0.085, 0.09, 0.085) if depth_index > current_depth else Color(0.11, 0.11, 0.10)

            var button := _make_button(label_text, center - NODE_SIZE * 0.5, NODE_SIZE, tint, tint.lightened(0.18), enabled)
            button.add_theme_font_size_override("font_size", 11 if not is_boss else 12)
            button.tooltip_text = "Этап %d: %s" % [depth_index + 1, "Логово Лешего" if is_boss else node_data["title"]]
            if is_current:
                var current_type := "boss" if is_boss else str(node_data["type"])
                var current_title := "Логово Лешего" if is_boss else str(node_data["title"])
                button.pressed.connect(_on_route_node_pressed.bind(depth_index, option_index, current_type, current_title))

    var footer_text := "ЭТАП %d ИЗ 10  •  %s" % [current_depth + 1, REGION_INFO[current_region]["open"]] if current_depth < BOSS_DEPTH else String(REGION_INFO[current_region]["done"])
    _add_label(footer_text, Vector2(24, view_size.y - 82), Vector2(view_size.x - 48, 26), 14, Color(0.85, 0.72, 0.50), HORIZONTAL_ALIGNMENT_CENTER)
    var message_text := status_message if not status_message.is_empty() else "Бой — награда после зачистки. Алтарь, тайник и источник дают другие бонусы."
    _add_label(message_text, Vector2(30, view_size.y - 53), Vector2(view_size.x - 60, 34), 12, Color(0.66, 0.64, 0.58), HORIZONTAL_ALIGNMENT_CENTER)
    queue_redraw()

func _nodes_for_depth(depth: int) -> Array:
    if depth == BOSS_DEPTH:
        return [{"type": "boss", "title": REGION_INFO[current_region]["boss_node"], "short": "БОСС"}]
    var stages: Array = DEAD_CITY_STAGES if current_region == "dead_city" else FOREST_STAGES
    if depth < 0 or depth >= stages.size():
        return []
    return stages[depth]

func _node_center(depth: int, option_index: int) -> Vector2:
    var view_size := get_viewport_rect().size
    var left := 49.0
    var right := view_size.x - 49.0
    var x := left + (right - left) * float(depth) / float(BOSS_DEPTH)
    var nodes := _nodes_for_depth(depth)
    var count := nodes.size()
    var y := view_size.y * 0.51
    if count == 2:
        y = view_size.y * (0.38 if option_index == 0 else 0.64)
    elif count >= 3:
        if option_index == 0:
            y = view_size.y * 0.28
        elif option_index == 1:
            y = view_size.y * 0.51
        else:
            y = view_size.y * 0.74
    return Vector2(x, y)

func _draw() -> void:
    var view_size := size
    draw_rect(Rect2(Vector2.ZERO, view_size), Color(0.035, 0.033, 0.030, 1.0), true)
    draw_rect(Rect2(Vector2(12, 12), view_size - Vector2(24, 24)), Color(0.35, 0.27, 0.17, 0.75), false, 2.0)
    draw_line(Vector2(35, 96), Vector2(view_size.x - 35, 96), Color(0.32, 0.25, 0.17, 0.8), 1.0)

    if view_mode != "forest":
        return

    # Draw a branching route graph behind the node buttons.
    for depth in range(BOSS_DEPTH):
        var from_nodes := _nodes_for_depth(depth)
        var to_nodes := _nodes_for_depth(depth + 1)
        for from_index in range(from_nodes.size()):
            var targets: Array[int] = []
            if to_nodes.size() == 1:
                targets.append(0)
            elif from_nodes.size() == 1:
                for target_index in range(to_nodes.size()):
                    targets.append(target_index)
            else:
                targets.append(mini(from_index, to_nodes.size() - 1))
                targets.append(mini(from_index + 1, to_nodes.size() - 1))
            for to_index in targets:
                var line_color := Color(0.25, 0.27, 0.24, 0.8)
                var line_width := 1.5
                if depth < selected_path.size() and int(selected_path[depth]) == from_index:
                    line_color = Color(0.62, 0.46, 0.24, 0.85)
                    line_width = 2.4
                draw_line(_node_center(depth, from_index), _node_center(depth + 1, to_index), line_color, line_width, true)

func _clear_children() -> void:
    for child in get_children():
        remove_child(child)
        child.queue_free()

func _add_label(text_value: String, pos: Vector2, label_size: Vector2, font_size: int, color: Color, alignment: int) -> Label:
    var label := Label.new()
    label.text = text_value
    label.position = pos
    label.size = label_size
    label.horizontal_alignment = alignment
    label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
    label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    label.add_theme_font_size_override("font_size", font_size)
    label.add_theme_color_override("font_color", color)
    label.mouse_filter = Control.MOUSE_FILTER_IGNORE
    add_child(label)
    return label

func _make_button(text_value: String, pos: Vector2, button_size: Vector2, color: Color, hover_color: Color, enabled: bool) -> Button:
    var button := Button.new()
    button.text = text_value
    button.position = pos
    button.size = button_size
    button.disabled = not enabled
    button.focus_mode = Control.FOCUS_ALL
    button.add_theme_stylebox_override("normal", _style_box(color, Color(0.38, 0.31, 0.21, 0.9)))
    button.add_theme_stylebox_override("hover", _style_box(hover_color, Color(0.77, 0.59, 0.32, 1.0)))
    button.add_theme_stylebox_override("pressed", _style_box(hover_color.darkened(0.12), Color(0.77, 0.59, 0.32, 1.0)))
    button.add_theme_stylebox_override("disabled", _style_box(color.darkened(0.12), Color(0.22, 0.21, 0.19, 0.65)))
    button.add_theme_color_override("font_color", Color(0.89, 0.82, 0.68))
    button.add_theme_color_override("font_hover_color", Color(1.0, 0.91, 0.70))
    button.add_theme_color_override("font_disabled_color", Color(0.38, 0.37, 0.34))
    add_child(button)
    return button

func _style_box(bg: Color, border: Color) -> StyleBoxFlat:
    var style := StyleBoxFlat.new()
    style.bg_color = bg
    style.border_color = border
    style.set_border_width_all(2)
    style.set_corner_radius_all(7)
    style.content_margin_left = 8.0
    style.content_margin_right = 8.0
    style.content_margin_top = 5.0
    style.content_margin_bottom = 5.0
    return style

func _node_color(node_type: String) -> Color:
    match node_type:
        "combat":
            return Color(0.21, 0.16, 0.12)
        "elite":
            return Color(0.30, 0.10, 0.09)
        "healing":
            return Color(0.10, 0.23, 0.18)
        "treasure":
            return Color(0.29, 0.23, 0.10)
        "shrine":
            return Color(0.19, 0.19, 0.31)
        "event":
            return Color(0.18, 0.18, 0.20)
        "boss":
            return Color(0.38, 0.08, 0.06)
        _:
            return Color(0.16, 0.15, 0.13)

func _on_region_button_pressed(region_id: String) -> void:
    region_selected.emit(region_id)

func _on_route_node_pressed(depth: int, option_index: int, node_type: String, title: String) -> void:
    route_node_selected.emit(depth, option_index, node_type, title)

func _on_return_to_regions_pressed() -> void:
    return_to_regions.emit()

func _on_back_pressed() -> void:
    back_requested.emit()

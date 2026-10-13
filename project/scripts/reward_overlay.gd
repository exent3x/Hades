extends Control

var game: Node = null

func set_game(value: Node) -> void:
    game = value

func _ready() -> void:
    $Panel/Choices/Damage.pressed.connect(func(): _choose(1))
    $Panel/Choices/Health.pressed.connect(func(): _choose(2))
    $Panel/Choices/Dash.pressed.connect(func(): _choose(3))

func _unhandled_input(event: InputEvent) -> void:
    if not visible or not event is InputEventKey or not event.pressed or event.echo:
        return
    if event.keycode == KEY_1:
        _choose(1)
    elif event.keycode == KEY_2:
        _choose(2)
    elif event.keycode == KEY_3:
        _choose(3)

func show_reward(title: String) -> void:
    $Panel/Title.text = title
    show()

func _choose(choice: int) -> void:
    if game != null:
        game.call("apply_reward", choice)

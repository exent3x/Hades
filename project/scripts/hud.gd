extends Control

var game: Node = null
var player: Node = null
var message_time := 0.0

func set_game(value: Node) -> void:
	game = value

func _process(delta: float) -> void:
	if game == null:
		return
	player = game.call("get_player") as Node
	if player != null and is_instance_valid(player):
		$Top/HP.value = float(player.call("get_health"))
		$Top/HP.max_value = float(player.call("get_max_health"))
		$Top/HPText.text = "ЖИЗНЬ  %d / %d" % [int(player.call("get_health")), int(player.call("get_max_health"))]
		$Top/Stats.text = "АТАКА x%.2f    ДЭШ %.2fs" % [float(player.call("get_attack_damage_multiplier")), float(player.call("get_dash_cooldown"))]
		var cd: float = float(player.call("get_skill_cooldown_left"))
		$Bottom/Skill.text = "Q — ОГНЕННЫЙ КРУГ  " + ("ГОТОВ" if cd <= 0.0 else ("%.1fс" % cd))
	if game.has_method("get_glory"):
		$Top/Glory.text = "ВЕЧНАЯ СЛАВА: %d" % int(game.call("get_glory"))
	if message_time > 0.0:
		message_time -= delta
		if message_time <= 0.0:
			$Center/Message.text = ""

func set_mode(mode: String, title: String) -> void:
	$Top/Title.text = title
	$Top/Mode.text = mode.to_upper()

func set_glory(value: int) -> void:
	$Top/Glory.text = "ВЕЧНАЯ СЛАВА: %d" % value

func show_message(text: String, duration: float = 3.0) -> void:
	$Center/Message.text = text
	message_time = duration

extends Node3D
## Визуал персонажа из GLB-модели (скелет и анимации сделаны пайплайном rigtool).
## Родитель — C++ ядро (CharacterBody3D). Ядро каждый кадр зовёт set_tint(Color),
## а этот скрипт опрашивает ядро и выбирает анимацию.
##
## Герой : idle / walk / dash / stun / attack_1..3 / throw / hit / death
## Босс  : idle / walk / melee / throw / cast / slam / grab / hit / death
##         Замах босса "прокручивается" по get_windup_progress(), чтобы удар
##         совпадал с моментом срабатывания способности в C++.

const TURN_SPEED := 14.0
const SPRUCE_BASE := Vector3(0.6215, -0.0738, -0.0972)   # основание ели в координатах модели

# действие босса -> [анимация, момент удара внутри неё (сек)]
const BOSS_ACTION_ANIMS := {
    "melee": ["melee", 0.75],
    "throw": ["throw", 0.90],
    "roots": ["slam", 0.70],
    "mushrooms": ["slam", 0.70],
    "birds": ["cast", 0.70],
    "summon": ["cast", 0.70],
    "shrink": ["cast", 0.70],
    "grab_throw": ["grab", 0.50],
}

# Кощей: действие -> [анимация, момент срабатывания внутри неё (сек)]
const KOSCHEI_ACTION_ANIMS := {
    "swipe": ["claw", 0.45],
    "combo": ["combo", 0.40],
    "lunge": ["lunge", 0.55],
    "bolts": ["point", 0.50],
    "curse": ["curse", 0.70],
    "storm": ["storm", 0.80],
    "nova": ["nova", 0.90],
    "summon": ["summon", 1.00],
    "shield": ["shield", 0.60],
    "jump": ["jump_crouch", 0.50],
    "jump_air": ["jump_air", 0.80],
    "jump_land": ["jump_land", 0.45],
}

var kind := "hero"
var shield_node: MeshInstance3D = null
var core: Node = null
var model: Node3D = null
var anim: AnimationPlayer = null
var materials: Array = []

var yaw := 0.0
var dead := false
var detached := false
var lock_left := 0.0
var last_health := -1.0
var hit_cooldown := 0.0
var was_attacking := false
var was_skill := false
var scrub_name := ""
var scrub_end := 0.0

func setup(p_kind: String, scene: PackedScene, model_scale: float, feet_offset: float) -> bool:
    kind = p_kind
    if scene == null:
        return false
    model = scene.instantiate() as Node3D
    if model == null:
        return false
    model.scale = Vector3.ONE * model_scale
    model.position = Vector3(0.0, feet_offset, 0.0)
    add_child(model)

    anim = model.find_child("AnimationPlayer", true, false) as AnimationPlayer
    if anim != null:
        for n in ["idle", "walk", "dash", "stun"]:
            if anim.has_animation(n):
                anim.get_animation(n).loop_mode = Animation.LOOP_LINEAR
        if anim.has_animation("idle"):
            anim.play("idle")

    # Свои копии материалов, чтобы тонировать каждого персонажа отдельно.
    for node in model.find_children("*", "MeshInstance3D", true, false):
        var mi := node as MeshInstance3D
        var src: Material = mi.get_active_material(0)
        if src == null:
            continue
        var dup := src.duplicate() as Material
        if dup is BaseMaterial3D:
            var bm := dup as BaseMaterial3D
            bm.emission_enabled = true          # сразу включаем: переключение потом вызывало бы перекомпиляцию шейдера
            bm.emission = Color(1.0, 0.85, 0.7)
            bm.emission_energy_multiplier = 0.0
        mi.material_override = dup
        materials.append(dup)
    if kind == "koschei":
        _setup_koschei()
    return true

func _setup_koschei() -> void:
    # Меч-кладенец летает отдельно (KladenetsSword3D), поэтому меш на теле прячем.
    var sword := model.find_child("Sword", true, false)
    if sword != null:
        (sword as Node3D).visible = false
    # Золотой пузырь временной неуязвимости.
    var sphere := SphereMesh.new()
    sphere.radius = 2.2
    sphere.height = 4.4
    var mat := StandardMaterial3D.new()
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    mat.albedo_color = Color(1.0, 0.86, 0.35, 0.22)
    sphere.material = mat
    shield_node = MeshInstance3D.new()
    shield_node.mesh = sphere
    shield_node.position = Vector3(0.0, 1.5, 0.0)
    shield_node.visible = false
    add_child(shield_node)

func _update_shield() -> void:
    if shield_node == null:
        return
    var on: bool = core.has_method("is_invulnerable") and bool(core.call("is_invulnerable"))
    shield_node.visible = on
    if on:
        var pulse := 1.0 + 0.05 * sin(Time.get_ticks_msec() * 0.012)
        shield_node.scale = Vector3.ONE * pulse

## Тонировка из C++: значения > 1 трактуются как вспышка попадания.
func set_tint(c: Color) -> void:
    var m := maxf(c.r, maxf(c.g, c.b))
    var tint := c
    var flash := 0.0
    if m > 1.02:
        flash = m - 1.0
        tint = Color(c.r / m, c.g / m, c.b / m, 1.0)
    for mat in materials:
        var bm := mat as BaseMaterial3D
        if bm == null:
            continue
        bm.albedo_color = tint
        bm.emission_energy_multiplier = flash * 1.4

func _process(delta: float) -> void:
    if model == null or anim == null or detached:
        return
    if core == null:
        core = get_parent()
    if core == null or not is_instance_valid(core):
        return
    hit_cooldown = maxf(0.0, hit_cooldown - delta)
    lock_left = maxf(0.0, lock_left - delta)
    if kind == "hero":
        _process_hero(delta)
    else:
        _process_boss(delta)

func _turn_to(dir: Vector3, delta: float) -> void:
    dir.y = 0.0
    if dir.length_squared() < 0.0001:
        return
    yaw = lerp_angle(yaw, atan2(dir.x, dir.z), 1.0 - exp(-TURN_SPEED * delta))
    rotation.y = yaw

func _length(n: String) -> float:
    return anim.get_animation(n).length if anim.has_animation(n) else 0.0

func _play(n: String, speed: float = 1.0, blend: float = 0.05) -> void:
    if not anim.has_animation(n):
        return
    anim.speed_scale = speed
    anim.play(n, blend)

func _loop(n: String, speed: float = 1.0) -> void:
    if not anim.has_animation(n):
        return
    anim.speed_scale = speed
    if anim.current_animation != n or not anim.is_playing():
        anim.play(n, 0.15)

func _horizontal_speed() -> float:
    var v: Vector3 = core.get("velocity")
    return Vector2(v.x, v.z).length()

# ------------------------------------------------------------------ герой

func _process_hero(delta: float) -> void:
    _turn_to(core.call("get_facing"), delta)

    if bool(core.call("is_dead")):
        if not dead:
            dead = true
            _play("death", 1.0, 0.05)
        return

    var health: float = core.call("get_health")
    var stun: float = core.call("get_stun_left")
    var attacking: bool = core.call("is_attacking")
    var skill: bool = core.call("is_using_skill")
    var dashing: bool = core.call("is_dashing")
    var combo: int = core.call("get_combo_step")

    if attacking and not was_attacking:
        var attack_name := "attack_%d" % clampi(combo, 1, 3)
        _play(attack_name, 1.0, 0.03)
        lock_left = _length(attack_name) * 0.85
    if skill and not was_skill:
        _play("throw", 1.0, 0.03)
        lock_left = _length("throw") * 0.85
    was_attacking = attacking
    was_skill = skill

    if last_health >= 0.0 and health < last_health - 0.01 and hit_cooldown <= 0.0 and lock_left <= 0.0 and stun <= 0.0:
        _play("hit", 1.0, 0.02)
        lock_left = _length("hit")
        hit_cooldown = 0.5
    last_health = health

    if stun > 0.0:
        lock_left = 0.0
        _loop("stun")
        return
    if dashing:
        lock_left = 0.0
        _loop("dash")
        return
    if lock_left > 0.0:
        return

    var speed := _horizontal_speed()
    if speed > 0.4:
        _loop("walk", clampf(speed / 4.6, 0.6, 1.4))
    else:
        _loop("idle")

# ------------------------------------------------------------------ босс

func _process_boss(delta: float) -> void:
    _turn_to(core.call("get_facing_dir"), delta)
    _update_shield()
    var action: String = core.call("get_action_name")
    var health: float = core.call("get_health")
    var amap: Dictionary = KOSCHEI_ACTION_ANIMS if kind == "koschei" else BOSS_ACTION_ANIMS

    if action != "none":
        # Замах: поза прокручивается по прогрессу замаха.
        var info: Array = amap.get(action, ["cast", 0.70])
        var n: String = info[0]
        if scrub_name != n:
            scrub_name = n
            scrub_end = float(info[1])
            anim.speed_scale = 1.0
            anim.play(n, 0.0)
            anim.pause()
        anim.seek(float(core.call("get_windup_progress")) * scrub_end, true)
        lock_left = 0.0
        last_health = health
        return

    if scrub_name != "":
        # Замах закончился (способность сработала): доигрываем анимацию с момента удара.
        var finished_name := scrub_name
        scrub_name = ""
        anim.speed_scale = 1.0
        anim.play(finished_name, 0.0)
        anim.seek(scrub_end, true)
        lock_left = maxf(0.0, _length(finished_name) - scrub_end)
        last_health = health
        return

    if last_health >= 0.0 and health < last_health - 0.01 and hit_cooldown <= 0.0 and lock_left <= 0.0:
        _play("hit", 1.0, 0.03)
        lock_left = _length("hit")
        hit_cooldown = 0.8
    last_health = health

    if lock_left > 0.0:
        return

    var speed := _horizontal_speed()
    var walk_ref := 2.6 if kind == "koschei" else 1.3
    if speed > 0.2:
        _loop("walk", clampf(speed / walk_ref, 0.6, 1.8))
    else:
        _loop("idle")

# ------------------------------------------------------------------ ель Лешего

func set_spruce_visible(v: bool) -> void:
    if model == null:
        return
    var s := model.find_child("Spruce", true, false)
    if s != null:
        (s as Node3D).visible = v

## Копия меша меча-кладенца (для летающего меча Кощея). Остриё — +Z, центр клинка — (0,0,0).
func make_sword_clone() -> Node3D:
    if model == null:
        return null
    var src := model.find_child("Sword", true, false) as MeshInstance3D
    if src == null:
        return null
    var holder := Node3D.new()
    var mi := MeshInstance3D.new()
    mi.mesh = src.mesh
    mi.scale = model.scale
    holder.add_child(mi)
    return holder

## Копия меша ели (для летящего снаряда). Основание — в точке (0,0,0).
func make_spruce_clone() -> Node3D:
    if model == null:
        return null
    var src := model.find_child("Spruce", true, false) as MeshInstance3D
    if src == null:
        return null
    var holder := Node3D.new()
    var mi := MeshInstance3D.new()
    mi.mesh = src.mesh
    mi.scale = model.scale
    mi.position = -SPRUCE_BASE * model.scale.x
    holder.add_child(mi)
    return holder

# ------------------------------------------------------------------ смерть босса

## Ядро сейчас освободится (queue_free): переносим визуал к обёртке и играем смерть.
func detach_for_death(new_parent: Node3D) -> void:
    if detached or model == null:
        return
    var xf := global_transform
    var old_parent := get_parent()
    if old_parent != null:
        old_parent.remove_child(self)
    new_parent.add_child(self)
    global_transform = xf
    detached = true
    dead = true
    if shield_node != null:
        shield_node.visible = false
    set_tint(Color(1, 1, 1, 1))
    if anim != null:
        anim.speed_scale = 1.0
        anim.play("death", 0.1)

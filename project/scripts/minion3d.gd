extends Node3D
## Обёртка вокруг C++ MinionController3D (лешонок — приспешник босса).

const Util := preload("res://scripts/billboard_util.gd")
const SPRITE_PATH := "res://art/sprites/minion_leshonok.png"
const SPRITE_HEIGHT_M := 1.0
const SKELETON_SPRITE_PATH := "res://art/sprites/minion_skeleton.png"
const SKELETON_HEIGHT_M := 1.7

## "leshonok" (приспешник Лешего) или "skeleton" (призыв Кощея). Задаётся до add_child().
var variant := "leshonok"

var core: CharacterBody3D = null

func _ready() -> void:
    if not ClassDB.class_exists("MinionController3D"):
        push_error("MinionController3D не найден. Пересобери GDExtension (scons).")
        return
    core = ClassDB.instantiate("MinionController3D") as CharacterBody3D
    if core == null:
        return

    core.collision_layer = 8
    core.collision_mask = 1

    var skeleton := variant == "skeleton"
    core.add_child(Util.make_shadow(0.6 if skeleton else 0.45, 0.5))
    core.add_child(Util.make_sprite(SKELETON_SPRITE_PATH if skeleton else SPRITE_PATH, SKELETON_HEIGHT_M if skeleton else SPRITE_HEIGHT_M))
    if skeleton:
        # скелет крепче и быстрее лешонка
        core.call("configure", {"hp": 55.0, "speed": 3.0, "damage": 10.0, "cooldown": 0.8, "range": 1.05})

    var body_shape := CollisionShape3D.new()
    body_shape.name = "CollisionShape3D"
    var capsule := CapsuleShape3D.new()
    capsule.radius = 0.32 if skeleton else 0.3
    capsule.height = 1.5 if skeleton else 0.9
    body_shape.shape = capsule
    body_shape.position = Vector3(0, 0.75 if skeleton else 0.45, 0)
    core.add_child(body_shape)

    # Хёртбокс: в него попадают атаки героя.
    var hurtbox := Area3D.new()
    hurtbox.name = "Hurtbox"
    hurtbox.collision_layer = 4
    hurtbox.collision_mask = 2
    core.add_child(hurtbox)
    var hurt_shape := CollisionShape3D.new()
    hurt_shape.name = "CollisionShape3D"
    var sphere := SphereShape3D.new()
    sphere.radius = 0.75 if skeleton else 0.6
    hurt_shape.shape = sphere
    hurt_shape.position = Vector3(0, 0.8 if skeleton else 0.5, 0)
    hurtbox.add_child(hurt_shape)

    add_child(core)
    core.position = Vector3.ZERO

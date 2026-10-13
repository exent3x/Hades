extends RefCounted
## Фабрики объектов арены: снаряды, зоны опасности, приспешники.
## Логика живёт в C++ (Projectile3D, HazardZone3D, MinionController3D); здесь — сборка визуала.

const Util := preload("res://scripts/billboard_util.gd")
const FxScript := preload("res://scripts/hazard_fx.gd")
const MinionScript := preload("res://scripts/minion3d.gd")
const EnemyScript := preload("res://scripts/enemy3d.gd")
const SPRUCE_SPRITE := "res://art/sprites/spruce_proj.png"

static func _instantiate(class_name_: String) -> Node3D:
    if not ClassDB.class_exists(class_name_):
        push_error("%s не найден. Пересобери GDExtension (scons)." % class_name_)
        return null
    return ClassDB.instantiate(class_name_) as Node3D

## Камень героя (прямой полёт, бьёт врагов из группы "enemies").
static func make_stone(origin: Vector3, direction: Vector3, damage: float) -> Node3D:
    var proj := _instantiate("Projectile3D")
    if proj == null:
        return null
    proj.call("configure", {
        "kind": "stone", "arc": false, "from": origin, "direction": direction,
        "speed": 16.0, "max_distance": 14.0, "damage": damage, "radius": 0.25,
    })
    proj.add_child(Util.make_sphere(0.2, Color(0.55, 0.55, 0.60)))
    return proj

## Снаряд босса: "spruce" (ель) или "mushroom" (гриб навесом).
static func make_boss_projectile(data: Dictionary, boss_core: Object, spruce_visual: Node3D = null) -> Node3D:
    var proj := _instantiate("Projectile3D")
    if proj == null:
        return null
    proj.call("configure", data)

    var kind: String = data.get("kind", "")
    if kind == "spruce":
        if spruce_visual != null:
            proj.add_child(spruce_visual)
        else:
            proj.add_child(Util.make_sprite(SPRUCE_SPRITE, 2.2))
    elif kind == "mushroom":
        proj.add_child(_make_mushroom_body(0.6))
    elif kind == "bolt":
        # Снаряд Кощея: светящийся костяной сгусток (проклятие — фиолетовый, обычный — зеленоватый).
        var cursed := data.has("vuln_time") or data.has("weaken_time") or data.has("slow_time") or data.has("silence_time")
        var arrow := String(data.get("style", "")) == "arrow"
        var orb := Util.make_sphere(0.34 if cursed else (0.18 if arrow else 0.26), Color(0.62, 0.25, 0.85) if cursed else (Color(1.0, 0.75, 0.3) if arrow else Color(0.55, 1.0, 0.72)))
        var mat := (orb.mesh as SphereMesh).material as StandardMaterial3D
        mat.emission_enabled = true
        mat.emission = Color(0.7, 0.3, 1.0) if cursed else (Color(1.0, 0.6, 0.15) if arrow else Color(0.3, 1.0, 0.55))
        mat.emission_energy_multiplier = 2.2
        orb.position = Vector3(0, 0.0, 0)
        proj.add_child(orb)

    # Только снаряды, о приземлении которых босс должен знать (ель, гриб); у болтов Кощея такого метода нет.
    if boss_core != null and is_instance_valid(boss_core) and boss_core.has_method("on_projectile_landed"):
        proj.connect("landed", Callable(boss_core, "on_projectile_landed"))
    return proj

## Зона опасности: "root" / "birds" / "mushroom".
static func make_hazard(kind: String, pos: Vector3, data: Dictionary) -> Node3D:
    var hz := _instantiate("HazardZone3D")
    if hz == null:
        return null
    hz.call("configure", data)
    hz.position = Vector3(pos.x, 0.0, pos.z)

    var radius := float(data.get("radius", 1.0))
    var active := float(data.get("active", 1.0))

    var fx := Node3D.new()
    fx.name = "Fx"
    fx.set_script(FxScript)

    var disc: MeshInstance3D
    var parts: Array = []

    match kind:
        "root":
            disc = Util.make_disc(Color(0.45, 0.25, 0.08, 0.2))
            fx.add_child(disc)
            for i in range(7):
                var pivot := Node3D.new()
                var ang := randf() * TAU
                var dist := randf_range(0.0, radius * 0.7)
                pivot.position = Vector3(cos(ang) * dist, 0.0, sin(ang) * dist)
                pivot.rotation = Vector3(randf_range(-0.35, 0.35), randf() * TAU, randf_range(-0.35, 0.35))
                var tendril := Util.make_box(Vector3(0.14, 1.4, 0.14), Color(0.30, 0.18, 0.07, 1.0))
                tendril.position = Vector3(0.0, 0.7, 0.0)
                pivot.add_child(tendril)
                fx.add_child(pivot)
                parts.append(pivot)
        "birds":
            disc = Util.make_disc(Color(0.10, 0.08, 0.14, 0.2))
            fx.add_child(disc)
            for i in range(16):
                var bird := Node3D.new()
                bird.add_child(Util.make_box(Vector3(0.34, 0.03, 0.12), Color(0.04, 0.04, 0.05, 1.0)))
                fx.add_child(bird)
                parts.append({
                    "node": bird,
                    "ang": randf() * TAU,
                    "rad": randf_range(0.3, radius * 0.95),
                    "h": randf_range(1.0, 2.3),
                    "spd": randf_range(1.5, 3.0) * (1.0 if i % 2 == 0 else -1.0),
                })
        "blast":
            # Заклинание Кощея по области: телеграф-круг, затем столб тёмного пламени.
            disc = Util.make_disc(Color(0.55, 0.18, 0.75, 0.2))
            fx.add_child(disc)
            var pillar_mesh := CylinderMesh.new()
            pillar_mesh.top_radius = 1.0
            pillar_mesh.bottom_radius = 1.0
            pillar_mesh.height = 4.0
            var pillar_mat := StandardMaterial3D.new()
            pillar_mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
            pillar_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
            pillar_mat.albedo_color = Color(0.72, 0.35, 1.0, 0.55)
            pillar_mesh.material = pillar_mat
            var pillar := Node3D.new()
            var pillar_mi := MeshInstance3D.new()
            pillar_mi.mesh = pillar_mesh
            pillar_mi.position = Vector3(0, 2.0, 0)
            pillar.add_child(pillar_mi)
            pillar.scale = Vector3(radius, 0.01, radius)
            pillar.visible = false
            fx.add_child(pillar)
            parts.append(pillar)
        "miasma":
            # Ядовитое поле: тусклый зелёно-фиолетовый круг и всплывающие огоньки.
            disc = Util.make_disc(Color(0.30, 0.55, 0.30, 0.22))
            fx.add_child(disc)
            for i in range(12):
                var wisp := Util.make_sphere(0.13, Color(0.55, 1.0, 0.65))
                var wisp_mat := (wisp.mesh as SphereMesh).material as StandardMaterial3D
                wisp_mat.emission_enabled = true
                wisp_mat.emission = Color(0.35, 1.0, 0.5)
                wisp_mat.emission_energy_multiplier = 1.6
                fx.add_child(wisp)
                parts.append({"node": wisp, "ang": randf() * TAU, "rad": randf_range(0.2, radius * 0.9), "ph": randf() * TAU})
        "mushroom":
            disc = Util.make_disc(Color(0.60, 0.22, 0.70, 0.2))
            fx.add_child(disc)
            var body := _make_mushroom_body(1.0)
            fx.add_child(body)
            parts.append(body)
        _:
            disc = Util.make_disc(Color(1, 0, 0, 0.2))
            fx.add_child(disc)

    disc.scale = Vector3(radius, 1.0, radius)
    fx.call("setup", kind, radius, active, disc, parts)
    hz.add_child(fx)
    return hz

## Леснóй приспешник (обёртка сама создаст C++ MinionController3D).
static func make_minion(variant: String = "leshonok") -> Node3D:
    var minion := Node3D.new()
    minion.name = "Skeleton" if variant == "skeleton" else "Minion"
    minion.set_script(MinionScript)
    minion.set("variant", variant)
    return minion

## Враг 3D-комнаты: role = brute / raider / jumper / archer (+ elite). Параметры задаются до add_child().
static func make_enemy(role: String, elite: bool, hp_mult: float, dmg_mult: float, arena_half: Vector2) -> Node3D:
    var enemy := Node3D.new()
    enemy.name = ("Elite_" if elite else "") + role.capitalize()
    enemy.set_script(EnemyScript)
    enemy.set("role", role)
    enemy.set("elite", elite)
    enemy.set("hp_mult", hp_mult)
    enemy.set("dmg_mult", dmg_mult)
    enemy.set("arena_half", arena_half)
    return enemy

static func _make_mushroom_body(scale_factor: float) -> Node3D:
    var pivot := Node3D.new()

    var stalk_mesh := CylinderMesh.new()
    stalk_mesh.top_radius = 0.09
    stalk_mesh.bottom_radius = 0.13
    stalk_mesh.height = 0.5
    var stalk_mat := StandardMaterial3D.new()
    stalk_mat.albedo_color = Color(0.90, 0.85, 0.72)
    stalk_mesh.material = stalk_mat
    var stalk := MeshInstance3D.new()
    stalk.mesh = stalk_mesh
    stalk.position = Vector3(0.0, 0.25, 0.0)
    pivot.add_child(stalk)

    var cap := Util.make_sphere(0.34, Color(0.80, 0.14, 0.10))
    cap.position = Vector3(0.0, 0.58, 0.0)
    cap.scale = Vector3(1.0, 0.65, 1.0)
    pivot.add_child(cap)

    pivot.scale = Vector3.ONE * scale_factor
    return pivot

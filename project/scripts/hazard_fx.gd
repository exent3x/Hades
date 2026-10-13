extends Node3D
## Визуал HazardZone3D (корни / птицы / гриб). Логика — в C++, здесь только анимация
## по состоянию родителя: get_state_name() и get_progress().

var kind := ""
var radius := 1.0
var active_time := 1.0
var disc: MeshInstance3D = null
var parts: Array = []
var t := 0.0
var hz = null

func setup(p_kind: String, p_radius: float, p_active: float, p_disc: MeshInstance3D, p_parts: Array) -> void:
    kind = p_kind
    radius = p_radius
    active_time = p_active
    disc = p_disc
    parts = p_parts

func _ready() -> void:
    hz = get_parent()

func _set_alpha(a: float) -> void:
    if disc == null:
        return
    var mat := (disc.mesh as CylinderMesh).material as StandardMaterial3D
    var c := mat.albedo_color
    c.a = a
    mat.albedo_color = c

func _process(delta: float) -> void:
    t += delta
    if hz == null or not is_instance_valid(hz) or not hz.has_method("get_state_name"):
        return
    var state: String = hz.call("get_state_name")
    var p: float = hz.call("get_progress")
    match kind:
        "root":
            _anim_root(state, p)
        "birds":
            _anim_birds(state, p, delta)
        "mushroom":
            _anim_mushroom(state, p)
        "blast":
            _anim_blast(state, p)
        "miasma":
            _anim_miasma(state, p)

func _anim_root(state: String, p: float) -> void:
    if state == "telegraph":
        _set_alpha(0.10 + 0.22 * p)
        for n in parts:
            n.scale = Vector3(1.0, 0.04 + 0.06 * p, 1.0)
        return
    var life := p * active_time
    var grow := clampf(life / 0.25, 0.0, 1.0)
    var retract := clampf((1.0 - p) * active_time / 0.4, 0.0, 1.0)
    var k := maxf(0.04, minf(grow, retract))
    var triggered := bool(hz.call("is_triggered"))
    _set_alpha(0.30 if triggered else 0.14)
    for i in range(parts.size()):
        parts[i].scale = Vector3(1.0, k * (1.0 + 0.06 * sin(t * 8.0 + i)), 1.0)

func _anim_birds(state: String, p: float, delta: float) -> void:
    var gather := 1.0
    if state == "telegraph":
        _set_alpha(0.10 + 0.14 * p)
        gather = 0.45 + 0.55 * p
    else:
        _set_alpha(0.22)
    for e in parts:
        e["ang"] += e["spd"] * delta
        var n: Node3D = e["node"]
        var r: float = e["rad"] * gather
        n.position = Vector3(cos(e["ang"]) * r, e["h"] * gather, sin(e["ang"]) * r)
        n.rotation.y = -e["ang"]
        n.rotation.z = sin(t * 18.0 + e["ang"] * 3.0) * 0.5

func _anim_mushroom(state: String, p: float) -> void:
    var body: Node3D = parts[0]
    if state == "telegraph":
        var s := 0.15 + 0.85 * (1.0 - pow(1.0 - p, 2.0))
        body.visible = true
        body.scale = Vector3.ONE * s
        body.rotation.z = sin(t * 30.0) * 0.05 * p   # дрожит перед взрывом
        disc.scale = Vector3(radius, 1.0, radius)
        _set_alpha(0.08 + 0.20 * p)
    else:
        body.visible = false
        var k := 0.35 + 0.65 * p
        disc.scale = Vector3(radius * k, 1.0, radius * k)
        _set_alpha(0.55 * (1.0 - p))

func _anim_blast(state: String, p: float) -> void:
    var pillar: Node3D = parts[0]
    if state == "telegraph":
        pillar.visible = false
        _set_alpha(0.10 + 0.28 * p)
        disc.scale = Vector3(radius, 1.0, radius)
    else:
        pillar.visible = true
        var k := minf(1.0, p * 5.0)
        pillar.scale = Vector3(radius * (1.0 - 0.25 * p), maxf(0.02, k), radius * (1.0 - 0.25 * p))
        var mat := ((pillar.get_child(0) as MeshInstance3D).mesh as CylinderMesh).material as StandardMaterial3D
        var c := mat.albedo_color
        c.a = 0.6 * (1.0 - p)
        mat.albedo_color = c
        _set_alpha(0.45 * (1.0 - p))

func _anim_miasma(state: String, p: float) -> void:
    var growth := p if state == "telegraph" else 1.0
    _set_alpha((0.08 + 0.16 * growth) + 0.05 * sin(t * 3.0))
    var fade := 1.0
    if state == "active":
        fade = clampf((1.0 - p) * active_time / 0.8, 0.0, 1.0)
    for e in parts:
        var n: Node3D = e["node"]
        var rise := fposmod(t * 0.5 + e["ph"], 1.0)
        n.position = Vector3(cos(e["ang"] + t * 0.4) * e["rad"], 0.2 + rise * 1.6, sin(e["ang"] + t * 0.4) * e["rad"])
        n.scale = Vector3.ONE * (growth * fade * (1.0 - 0.6 * rise))

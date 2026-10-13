extends RefCounted
## Общие помощники для 2.5D: спрайты-билборды, тени, диски-индикаторы.

## Текстура с мипмапами: большой арт не рябит при уменьшении.
static func make_texture(path: String) -> ImageTexture:
    var source := load(path) as Texture2D
    if source == null:
        push_error("Не найден спрайт: " + path)
        return null
    var image := source.get_image()
    image.generate_mipmaps()
    return ImageTexture.create_from_image(image)

## Спрайт-билборд, "стоящий" на полу: pivot — точка под ногами.
static func make_sprite(path: String, height_m: float) -> Sprite3D:
    var sprite := Sprite3D.new()
    sprite.name = "Sprite"

    var tex := make_texture(path)
    if tex == null:
        return sprite
    sprite.texture = tex

    sprite.pixel_size = height_m / float(tex.get_height())
    sprite.centered = true
    # Поднимаем картинку на пол-высоты (в пикселях), чтобы ноги были в точке (0,0,0).
    # Если спрайт окажется "утопленным" в пол — смени знак на минус.
    sprite.offset = Vector2(0.0, tex.get_height() * 0.5)
    sprite.billboard = BaseMaterial3D.BILLBOARD_ENABLED
    sprite.alpha_cut = SpriteBase3D.ALPHA_CUT_OPAQUE_PREPASS
    sprite.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
    sprite.shaded = false
    return sprite

## Мягкая круглая тень под персонажем.
static func make_shadow(radius: float, strength: float = 0.55) -> MeshInstance3D:
    var gradient := Gradient.new()
    gradient.set_color(0, Color(0, 0, 0, strength))
    gradient.set_color(1, Color(0, 0, 0, 0))

    var tex := GradientTexture2D.new()
    tex.gradient = gradient
    tex.fill = GradientTexture2D.FILL_RADIAL
    tex.fill_from = Vector2(0.5, 0.5)
    tex.fill_to = Vector2(1.0, 0.5)
    tex.width = 128
    tex.height = 128

    var material := StandardMaterial3D.new()
    material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    material.albedo_texture = tex
    material.no_depth_test = false

    var quad := QuadMesh.new()
    quad.size = Vector2(radius * 2.0, radius * 2.0)
    quad.material = material

    var shadow := MeshInstance3D.new()
    shadow.name = "Shadow"
    shadow.mesh = quad
    shadow.rotation_degrees = Vector3(-90, 0, 0)
    shadow.position = Vector3(0, 0.02, 0)
    return shadow

## Плоский диск единичного радиуса (масштабируется по X/Z) — для телеграфов и вспышек умений.
static func make_disc(color: Color, y: float = 0.04) -> MeshInstance3D:
    var cylinder := CylinderMesh.new()
    cylinder.top_radius = 1.0
    cylinder.bottom_radius = 1.0
    cylinder.height = 0.02
    cylinder.radial_segments = 48

    var material := StandardMaterial3D.new()
    material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    material.albedo_color = color
    cylinder.material = material

    var disc := MeshInstance3D.new()
    disc.mesh = cylinder
    disc.position = Vector3(0, y, 0)
    return disc

static func make_box(size: Vector3, color: Color) -> MeshInstance3D:
    var box := BoxMesh.new()
    box.size = size
    var material := StandardMaterial3D.new()
    material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    material.albedo_color = color
    box.material = material
    var mesh := MeshInstance3D.new()
    mesh.mesh = box
    return mesh

static func make_sphere(radius: float, color: Color) -> MeshInstance3D:
    var sphere := SphereMesh.new()
    sphere.radius = radius
    sphere.height = radius * 2.0
    var material := StandardMaterial3D.new()
    material.albedo_color = color
    material.roughness = 0.9
    sphere.material = material
    var mesh := MeshInstance3D.new()
    mesh.mesh = sphere
    return mesh

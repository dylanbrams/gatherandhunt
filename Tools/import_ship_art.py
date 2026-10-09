"""Imports a scanned ship (one mesh, several 4-frame texture sets) and builds its materials.

Run as an editor commandlet (see Docs/Design/ShipArt.md):

    UnrealEditor-Cmd.exe GatherAndHunt.uproject -run=pythonscript
        -script="Tools/import_ship_art.py" -EnablePlugins=PythonScriptPlugin -unattended

Re-running re-imports only the source files that changed, rebuilds the material
and keeps the material instances' other parameters. Set SOURCE below to import a different drop.
"""

import glob
import os

import unreal

SOURCE = r"D:\Source\Unreal\GatherAndHunt\Design\SPCSH_Green_V1"
SHIP = "SPCSH_Green"
SHIP_DIR = "/Game/Art/Ships/" + SHIP
MATERIAL_DIR = "/Game/Art/Ships"
MATERIAL_NAME = "M_ShipFrameCycle"

MESH_FILE = os.path.join(SOURCE, "5K_4KTx", "FBX", "SPCSH_Green.fbx")
BASE_TEXTURE = os.path.join(SOURCE, "5K_4KTx", "FBX", "SPCSH_Green_u1_v1_diffuse.png")
# Skin name -> folder under "Texture Animation". Frames sort by their -N suffix.
SKINS = {
    "Green": "Green Color Variations",
    "Gold": "Gold Color Variations",
    "Pink": "Pink Color Variations",
    "Purple": "Purple Color Variations",
}
FRAME_COUNT = 4
# Shape and timing of the hull lights (make_running_light_masks.py).
LIGHTS_FILE = os.path.join(SOURCE, "Running Lights", SHIP + "_RunningLights.png")
# Red: the cockpit. Green: the back of the tailfin. Both glow all the time.
GLOW_AREAS_FILE = os.path.join(SOURCE, "Running Lights", SHIP + "_GlowAreas.png")

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def import_file(filename, destination, name, options=None):
    # Skip sources that haven't changed since they were last imported.
    package_file = os.path.join(unreal.Paths.project_content_dir(), destination[len("/Game/"):], name + ".uasset")
    if os.path.exists(package_file) and os.path.getmtime(package_file) >= os.path.getmtime(filename):
        return unreal.load_asset("%s/%s" % (destination, name))

    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options is not None:
        task.options = options
    asset_tools.import_asset_tasks([task])
    path = "%s/%s" % (destination, name)
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError("Import failed: %s -> %s" % (filename, path))
    return asset


def import_mesh():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.static_mesh_import_data.combine_meshes = True
    options.static_mesh_import_data.auto_generate_collision = False
    return import_file(MESH_FILE, SHIP_DIR, "SM_" + SHIP, options)


def import_textures():
    textures = {"Base": [import_file(BASE_TEXTURE, SHIP_DIR + "/Textures", "T_%s_Base" % SHIP)]}
    for skin, folder in SKINS.items():
        files = sorted(glob.glob(os.path.join(SOURCE, "Texture Animation", folder, "*.png")))
        if len(files) != FRAME_COUNT:
            raise RuntimeError("%s: expected %d frames, found %d" % (folder, FRAME_COUNT, len(files)))
        textures[skin] = [
            import_file(f, SHIP_DIR + "/Textures", "T_%s_%s_%d" % (SHIP, skin, i + 1))
            for i, f in enumerate(files)
        ]
    return textures


def import_data_texture(filename, name):
    texture = import_file(filename, SHIP_DIR + "/Textures", name)
    # Its channels are numbers, not colour: keep them exact.
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
    eal.save_loaded_asset(texture)
    return texture


def build_material(default_texture, default_lights, default_areas):
    """Lit material that crossfades through four frame textures on a loop.

    Each frame holds, then fades to the next over the last `Blend` of its slot.
    Emissive is the running lights (one dot lit at a time, walking from the
    rear to the nose) plus the cockpit and the back of the tailfin, which glow
    in their painted colours all the time.
    """
    path = "%s/%s" % (MATERIAL_DIR, MATERIAL_NAME)
    if eal.does_asset_exist(path):
        material = unreal.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y, **props):
        expression = mel.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(source, target, target_input, source_output=""):
        if not mel.connect_material_expressions(source, source_output, target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def scalar(name, default, y, group="Animation"):
        return node(unreal.MaterialExpressionScalarParameter, -2200, y,
                    parameter_name=name, default_value=default, group=group)

    def binary(cls, a, b, x, y, **props):
        expression = node(cls, x, y, **props)
        if a is not None:
            link(a, expression, "A")
        if b is not None:
            link(b, expression, "B")
        return expression

    def unary(cls, source, x, y):
        expression = node(cls, x, y)
        link(source, expression, "")
        return expression

    # Position in the loop, 0..FRAME_COUNT.
    time = node(unreal.MaterialExpressionTime, -2200, -200)
    cycle = scalar("CycleSeconds", 2.0, -100)
    loops = binary(unreal.MaterialExpressionDivide, time, cycle, -2000, -200)
    position = binary(unreal.MaterialExpressionMultiply, unary(unreal.MaterialExpressionFrac, loops, -1850, -200),
                      None, -1700, -200, const_b=float(FRAME_COUNT))

    # Hold each frame, fading only over the last `Blend` of its slot.
    blend = binary(unreal.MaterialExpressionMax, scalar("Blend", 0.5, 0), None, -2000, 0, const_b=0.001)
    whole = unary(unreal.MaterialExpressionFloor, position, -1500, -260)
    part = unary(unreal.MaterialExpressionFrac, position, -1500, -140)
    hold = unary(unreal.MaterialExpressionOneMinus, blend, -1850, 60)
    fade = unary(unreal.MaterialExpressionSaturate,
                 binary(unreal.MaterialExpressionDivide,
                        binary(unreal.MaterialExpressionSubtract, part, hold, -1350, -140), blend, -1200, -140),
                 -1050, -140)
    eased = binary(unreal.MaterialExpressionAdd, whole, fade, -900, -200)

    color = None
    first = None
    for index in range(FRAME_COUNT):
        y = 200 + index * 300
        sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, y,
                      parameter_name="Frame%d" % (index + 1), texture=default_texture, group="Frames")
        # Distance round the loop from this frame; weight 1 at 0, 0 at one frame away.
        distance = unary(unreal.MaterialExpressionAbs,
                         binary(unreal.MaterialExpressionSubtract, eased, None, -900, y - 80, const_b=float(index)),
                         -750, y - 80)
        wrapped = binary(unreal.MaterialExpressionMin, distance,
                         binary(unreal.MaterialExpressionSubtract, None, distance, -600, y - 40,
                                const_a=float(FRAME_COUNT)), -450, y - 80)
        weight = unary(unreal.MaterialExpressionSaturate, unary(unreal.MaterialExpressionOneMinus, wrapped, -300, y - 80),
                       -150, y - 80)
        term = node(unreal.MaterialExpressionMultiply, 0, y)
        link(sample, term, "A", "RGB")
        link(weight, term, "B")
        if index == 0:
            first = sample
        color = term if color is None else binary(unreal.MaterialExpressionAdd, color, term, 200, y)

    # Running lights. The texture gives each dot its shape (R), the point in
    # the loop where it peaks (G) and the share of the loop it is lit for (B).
    black = unreal.load_asset("/Engine/EngineResources/Black")
    lights_group = "Running lights"
    dots = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 2600,
                parameter_name="RunningLights", texture=default_lights, group=lights_group,
                sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # The loop runs a little past the last dot, so there is a dark beat before
    # the light starts again at the rear instead of jumping straight back.
    loop = binary(unreal.MaterialExpressionMultiply,
                  unary(unreal.MaterialExpressionFrac,
                        binary(unreal.MaterialExpressionDivide, time,
                               scalar("RunningLightSeconds", 2.0, 2400, lights_group), -2000, 2400), -1850, 2400),
                  binary(unreal.MaterialExpressionAdd, scalar("RunningLightGap", 0.15, 2500, lights_group), None,
                         -2000, 2500, const_b=1.0), -1700, 2400)
    # How far the loop is from this dot's peak, in dot-turns: 0.5 is the hand-over to its neighbour.
    away = node(unreal.MaterialExpressionSubtract, -600, 2500)
    link(loop, away, "A")
    link(dots, away, "B", "G")
    turn = node(unreal.MaterialExpressionMax, -600, 2700, const_b=0.001)
    link(dots, turn, "A", "B")
    turns = binary(unreal.MaterialExpressionDivide, unary(unreal.MaterialExpressionAbs, away, -450, 2500), turn, -300, 2500)
    # Neighbours crossfade over RunningLightFade of a turn either side of the hand-over. 0 switches.
    fade = binary(unreal.MaterialExpressionMax, scalar("RunningLightFade", 0.25, 2600, lights_group), None,
                  -2000, 2600, const_b=0.001)
    level = unary(unreal.MaterialExpressionSaturate,
                  binary(unreal.MaterialExpressionDivide,
                         binary(unreal.MaterialExpressionSubtract,
                                binary(unreal.MaterialExpressionAdd, fade, None, -300, 2650, const_b=0.5),
                                turns, -150, 2550),
                         binary(unreal.MaterialExpressionMultiply, fade, None, -300, 2750, const_b=2.0), 0, 2550),
                  150, 2550)
    lights = node(unreal.MaterialExpressionMultiply, 300, 2600)
    link(dots, lights, "A", "R")
    link(level, lights, "B")

    # Far away a dot is smaller than a pixel and averages out to nothing, so
    # scale the emission up with distance, measured in ship sizes.
    distance = binary(unreal.MaterialExpressionDistance, node(unreal.MaterialExpressionWorldPosition, -900, 3000),
                      node(unreal.MaterialExpressionCameraPositionWS, -900, 3100), -700, 3000)
    sizes = binary(unreal.MaterialExpressionDivide, distance,
                   binary(unreal.MaterialExpressionMultiply, node(unreal.MaterialExpressionObjectRadius, -900, 3200), None,
                          -700, 3200, const_b=3.0), -550, 3000)
    boost = binary(unreal.MaterialExpressionMin,
                   binary(unreal.MaterialExpressionMax, sizes, None, -400, 3000, const_b=1.0),
                   scalar("RunningLightFarBoost", 8.0, 3300, lights_group), -250, 3000)

    # The top spine and the bottom line have their own colours (A is 1 on the
    # bottom), so a ship that has rolled over reads differently.
    light_tint = node(unreal.MaterialExpressionLinearInterpolate, -600, 3500)
    link(node(unreal.MaterialExpressionVectorParameter, -900, 3500, parameter_name="RunningLightColor",
              default_value=unreal.LinearColor(1.0, 0.85, 0.5, 1.0), group=lights_group), light_tint, "A", "RGB")
    link(node(unreal.MaterialExpressionVectorParameter, -900, 3700, parameter_name="RunningLightBottomColor",
              default_value=unreal.LinearColor(0.4, 0.7, 1.0, 1.0), group=lights_group), light_tint, "B", "RGB")
    link(dots, light_tint, "Alpha", "A")
    painted = node(unreal.MaterialExpressionMultiply, 600, 2600)
    link(lights, painted, "A")
    link(light_tint, painted, "B")
    painted = binary(unreal.MaterialExpressionMultiply, painted,
                     binary(unreal.MaterialExpressionMultiply, scalar("RunningLightStrength", 3.0, 3600, lights_group),
                            boost, 600, 3300), 750, 2600)

    # Under the lights the base colour stays on frame 1 (lights unlit), so the
    # frames whitening every light at once doesn't fight the running pattern.
    unlit = node(unreal.MaterialExpressionLinearInterpolate, 600, 600)
    link(color, unlit, "A")
    link(first, unlit, "B", "RGB")
    link(dots, unlit, "Alpha", "R")
    mel.connect_material_property(unlit, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(scalar("Roughness", 0.75, 1500, "Surface"), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(scalar("Metallic", 0.0, 1600, "Surface"), "", unreal.MaterialProperty.MP_METALLIC)

    # Glow areas: the cockpit (R) pulses, the back of the tailfin (G) is steady.
    # Each glows in the colour painted there, times its tint.
    areas_group = "Glow areas"
    areas = node(unreal.MaterialExpressionTextureSampleParameter2D, -900, 3800,
                 parameter_name="GlowAreas", texture=default_areas, group=areas_group,
                 sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    # Dips by CockpitPulseDepth once every CockpitPulseSeconds, in step with the cockpit's far glow.
    wave = binary(unreal.MaterialExpressionAdd,
                  binary(unreal.MaterialExpressionMultiply,
                         unary(unreal.MaterialExpressionSine,
                               binary(unreal.MaterialExpressionDivide, time,
                                      scalar("CockpitPulseSeconds", 2.0, 4000, areas_group), -2000, 4000),
                               -1850, 4000),
                         None, -1700, 4000, const_b=0.5), None, -1550, 4000, const_b=0.5)
    pulse = unary(unreal.MaterialExpressionOneMinus,
                  binary(unreal.MaterialExpressionMultiply, wave, scalar("CockpitPulseDepth", 0.6, 4100, areas_group),
                         -1400, 4000), -1250, 4000)

    def area(channel, level, strength_name, strength, tint_name, tint, y):
        amount = node(unreal.MaterialExpressionMultiply, -600, y)
        link(areas, amount, "A", channel)
        link(level, amount, "B")
        coloured = node(unreal.MaterialExpressionMultiply, -300, y)
        link(binary(unreal.MaterialExpressionMultiply, amount, scalar(strength_name, strength, y + 80, areas_group), -450, y),
             coloured, "A")
        link(node(unreal.MaterialExpressionVectorParameter, -900, y + 160, parameter_name=tint_name,
                  default_value=tint, group=areas_group), coloured, "B", "RGB")
        return coloured

    cockpit = area("R", pulse, "CockpitGlowStrength", 4.0, "CockpitGlowColor", unreal.LinearColor(1.0, 0.6, 0.45, 1.0), 4300)
    steady = scalar("TailGlowLevel", 1.0, 4700, areas_group)
    tail = area("G", steady, "TailGlowStrength", 4.0, "TailGlowColor", unreal.LinearColor(0.2, 1.0, 0.35, 1.0), 4700)
    glowing = binary(unreal.MaterialExpressionMultiply,
                     binary(unreal.MaterialExpressionAdd, cockpit, tail, -150, 4500), unlit, 0, 4500)
    mel.connect_material_property(binary(unreal.MaterialExpressionAdd, painted, glowing, 850, 1650), "",
                                  unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    return material


def build_instance(material, skin, frames, running_lights, glow_areas):
    name = "MI_%s_%s" % (SHIP, skin)
    path = "%s/%s" % (SHIP_DIR, name)
    if eal.does_asset_exist(path):
        instance = unreal.load_asset(path)
    else:
        instance = asset_tools.create_asset(name, SHIP_DIR, unreal.MaterialInstanceConstant,
                                            unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, material)
    for index in range(FRAME_COUNT):
        # A single-frame skin (the untouched scan) repeats its one texture.
        mel.set_material_instance_texture_parameter_value(instance, "Frame%d" % (index + 1), frames[index % len(frames)])
    mel.set_material_instance_texture_parameter_value(instance, "RunningLights", running_lights)
    mel.set_material_instance_texture_parameter_value(instance, "GlowAreas", glow_areas)
    mel.update_material_instance(instance)
    eal.save_loaded_asset(instance)
    return instance


def build_glow_material():
    """Unlit additive sprite: a soft round glow that can pulse or blink. Drawn over everything."""
    name = "M_ShipGlow"
    path = "%s/%s" % (MATERIAL_DIR, name)
    if eal.does_asset_exist(path):
        material = unreal.load_asset(path)
        mel.delete_all_material_expressions(material)
    else:
        material = asset_tools.create_asset(name, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    # A ship's lights show which way it points even when the hull hides them.
    material.set_editor_property("disable_depth_test", True)

    def node(cls, x, y, **props):
        expression = mel.create_material_expression(material, cls, x, y)
        for key, value in props.items():
            expression.set_editor_property(key, value)
        return expression

    def link(source, target, target_input, source_output=""):
        if not mel.connect_material_expressions(source, source_output, target, target_input):
            raise RuntimeError("Could not connect %s -> %s.%s" % (source.get_name(), target.get_name(), target_input))

    def chain(source, cls, x, y, **props):
        expression = node(cls, x, y, **props)
        link(source, expression, "A" if cls in (unreal.MaterialExpressionMultiply, unreal.MaterialExpressionSubtract,
                                                unreal.MaterialExpressionDivide, unreal.MaterialExpressionAdd) else "")
        return expression

    def scalar(name, default, y):
        return node(unreal.MaterialExpressionScalarParameter, -1400, y, parameter_name=name, default_value=default)

    # 1 at the centre of the sprite, 0 at its edge, eased by Falloff.
    offset = chain(node(unreal.MaterialExpressionTextureCoordinate, -1400, 0), unreal.MaterialExpressionSubtract,
                   -1200, 0, const_b=0.5)
    radius = chain(chain(offset, unreal.MaterialExpressionLength, -1050, 0), unreal.MaterialExpressionMultiply,
                   -900, 0, const_b=2.0)
    disc = chain(chain(radius, unreal.MaterialExpressionOneMinus, -750, 0), unreal.MaterialExpressionSaturate, -600, 0)
    shaped = node(unreal.MaterialExpressionPower, -450, 0)
    link(disc, shaped, "Base")
    link(scalar("Falloff", 2.0, 150), shaped, "Exp")

    # Pulse: once every PulseSeconds the glow dips by PulseDepth (0 is steady,
    # 1 goes out). PulseSharpness 1 is a smooth swell; higher keeps it dark
    # for most of the period and lit for a short flash, which reads as a blink.
    # PulseRate is the player's blink-speed option, set by the game.
    turns = node(unreal.MaterialExpressionDivide, -1200, 400)
    clock = node(unreal.MaterialExpressionMultiply, -1300, 350)
    link(node(unreal.MaterialExpressionTime, -1450, 300), clock, "A")
    link(scalar("PulseRate", 1.0, 350), clock, "B")
    link(clock, turns, "A")
    link(scalar("PulseSeconds", 2.0, 450), turns, "B")
    wave = chain(chain(chain(turns, unreal.MaterialExpressionSine, -1050, 400, period=1.0),
                       unreal.MaterialExpressionMultiply, -900, 400, const_b=0.5),
                 unreal.MaterialExpressionAdd, -750, 400, const_b=0.5)
    lit = node(unreal.MaterialExpressionPower, -600, 400)
    link(chain(wave, unreal.MaterialExpressionOneMinus, -700, 400), lit, "Base")
    link(scalar("PulseSharpness", 1.0, 650), lit, "Exp")
    dip = node(unreal.MaterialExpressionMultiply, -350, 400)
    link(chain(lit, unreal.MaterialExpressionOneMinus, -450, 400), dip, "A")
    link(scalar("PulseDepth", 0.0, 550), dip, "B")
    pulse = chain(dip, unreal.MaterialExpressionOneMinus, -250, 400)

    glow = node(unreal.MaterialExpressionMultiply, -250, 100)
    link(shaped, glow, "A")
    link(pulse, glow, "B")
    strong = node(unreal.MaterialExpressionMultiply, -100, 100)
    link(glow, strong, "A")
    link(scalar("Strength", 2.0, 700), strong, "B")
    # Set per sprite by the game: 0 close up, where the hull's own glow does the
    # work, rising to 1 once the sprite is at its minimum on-screen size.
    faded = node(unreal.MaterialExpressionMultiply, -25, 100)
    link(strong, faded, "A")
    link(scalar("Fade", 1.0, 950), faded, "B")
    strong = faded
    tinted = node(unreal.MaterialExpressionMultiply, 50, 100)
    link(strong, tinted, "A")
    link(node(unreal.MaterialExpressionVectorParameter, -1400, 800, parameter_name="Color",
              default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0)), tinted, "B", "RGB")
    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    mel.recompile_material(material)
    eal.save_loaded_asset(material)
    return material


def build_glow_instance(material, name, color, scalars):
    """Created once; after that its values belong to whoever tunes them in the editor."""
    path = "%s/%s" % (MATERIAL_DIR, name)
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    instance = asset_tools.create_asset(name, MATERIAL_DIR, unreal.MaterialInstanceConstant,
                                        unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, material)
    mel.set_material_instance_vector_parameter_value(instance, "Color", color)
    for key, value in scalars.items():
        mel.set_material_instance_scalar_parameter_value(instance, key, value)
    mel.update_material_instance(instance)
    eal.save_loaded_asset(instance)
    return instance


def main():
    mesh = import_mesh()
    textures = import_textures()
    running_lights = import_data_texture(LIGHTS_FILE, "T_%s_RunningLights" % SHIP)
    glow_areas = import_data_texture(GLOW_AREAS_FILE, "T_%s_GlowAreas" % SHIP)
    material = build_material(textures["Base"][0], running_lights, glow_areas)
    instances = {skin: build_instance(material, skin, frames, running_lights, glow_areas)
                 for skin, frames in textures.items()}
    mesh.set_material(0, instances["Green"])
    eal.save_loaded_asset(mesh)

    glow = build_glow_material()
    build_glow_instance(glow, "MI_ShipGlow_Cockpit", unreal.LinearColor(1.0, 0.12, 0.04, 1.0),
                        {"Strength": 3.0, "PulseDepth": 0.6, "PulseSeconds": 2.0})
    build_glow_instance(glow, "MI_ShipGlow_Tailfin", unreal.LinearColor(0.15, 1.0, 0.35, 1.0),
                        {"Strength": 1.5, "PulseDepth": 0.0, "Falloff": 1.5})
    # Seen from underneath: a darker red nose and a lighter green tail. The nose
    # also pulses twice as fast, so the underside doesn't depend on telling shades apart.
    build_glow_instance(glow, "MI_ShipGlow_NoseBelow", unreal.LinearColor(0.75, 0.0, 0.1, 1.0),
                        {"Strength": 2.5, "PulseDepth": 0.6, "PulseSeconds": 1.0})
    build_glow_instance(glow, "MI_ShipGlow_TailBelow", unreal.LinearColor(0.55, 1.0, 0.75, 1.0),
                        {"Strength": 2.0, "PulseDepth": 0.0, "Falloff": 1.5})
    # Seen from above only: off-white strobes on the wingtips. With the nose and
    # tail that makes four dots on top against two underneath.
    build_glow_instance(glow, "MI_ShipGlow_Wingtip", unreal.LinearColor(1.0, 0.94, 0.82, 1.0),
                        {"Strength": 2.5, "PulseDepth": 1.0, "PulseSharpness": 6.0, "PulseSeconds": 1.2})

    bounds = mesh.get_bounds()
    unreal.log("SHIPART mesh %s origin=%s extent=%s" % (mesh.get_path_name(), bounds.origin, bounds.box_extent))
    for skin, instance in instances.items():
        unreal.log("SHIPART skin %s %s" % (skin, instance.get_path_name()))


main()

"""Cuts the ship's painted lights out of its texture frames as one running-light texture.

The lights are the dots that brighten between frame 1 and frame 3 of every
skin. Each dot is placed on the hull through the mesh's UVs. Only the dots on
the centreline are kept: the spine along the top and the matching line along
the bottom. Each line is put in order from the rear and shares the loop out
equally, so one dot is lit at a time and the light walks to the nose; a line
with fewer dots holds each one longer, and both lines arrive together.

Channels of the texture (linear, not sRGB):
    R  the dot's shape, soft-edged.
    G  when in the loop (0..1) the dot is at its brightest.
    B  how much of the loop the dot is lit for.
    A  which line the dot is on: 0 the top spine, 1 the bottom. The material
       gives the two lines different colours.
G, B and A are flat across each dot and its surroundings, so only R needs
repainting to reshape or remove a dot.

Plain Python (needs numpy and Pillow), run after export_ship_uv.py:

    python Tools/make_running_light_masks.py

Writes <SOURCE>/Running Lights/<ship>_RunningLights.png (replacing what is
there), which import_ship_art.py picks up.

Also writes <ship>_GlowAreas.png beside it: the parts of the hull that glow all
the time. Red is the cockpit (the red-orange paint on top of the nose), green is
the tailfin's top point and back edge. Both are plain soft masks and can be repainted.
"""

import glob
import json
import os

import numpy as np
from PIL import Image, ImageFilter

Image.MAX_IMAGE_PIXELS = None

PROJECT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE = r"D:\Source\Unreal\GatherAndHunt\Design\SPCSH_Green_V1"
SHIP = "SPCSH_Green"
UV_FILE = os.path.join(PROJECT, "Saved", "ShipArt", "SM_%s_uv.json" % SHIP)
SKIN_FOLDERS = ["Green Color Variations", "Gold Color Variations", "Pink Color Variations", "Purple Color Variations"]
OUT_DIR = os.path.join(SOURCE, "Running Lights")

# Mesh axis the ship's length runs along, and which end the nose is at.
LENGTH_AXIS = 1
NOSE_AT_NEGATIVE = True
SIDE_AXIS = 0
UP_AXIS = 2

# A dot is on the centreline if it is within this share of the half-width of it.
CENTRE_SHARE = 0.1
# The bottom line of dots. False keeps the top spine only.
INCLUDE_BOTTOM = True
# Lit areas this many times the typical dot's size are panels (the cockpit), not dots.
PANEL_SIZE = 4.0

# The skin whose paint is read where colour matters (how white, how red).
REFERENCE_SKIN = "Green Color Variations"
# The cockpit is the paint on top of the hull, this far along it or further,
# that is redder than the hull.
COCKPIT_FROM = 0.78
COCKPIT_REDNESS = 1.12  # Red over green.
# The tailfin is what stands this far up the ship's height. It glows at its top
# point and along its back edge, fading smoothly away from both.
FIN_ABOVE = 0.72
FIN_TIP_SIZE = 0.3  # Reach of the glow round the top point, as a share of the fin's height.
FIN_EDGE_WIDTH = 0.1  # Reach of the glow in from the back edge, as a share of the fin's length.
AREA_SOFTNESS = 6.0  # Blur on the glow areas' edges, in output texels.

MASK_SIZE = 1024  # Output resolution. Kept uncompressed in the project, so keep it small.
GRID = 1024  # Working resolution for placing blobs on the hull.
RISE = 30  # How much brighter (0-255) than frame 1 counts as lit.
WHITE = 165  # A light's brightest part is at least this white (0-255) in frame 3; lighter seam strokes are not.
SPECK = 0.4  # Dots under this share of the typical dot's size are stray specks of paint.
MERGE_GAP = 0.025  # Dots this close along the hull (as a share of its length) are one light cut by a seam.
SOFTNESS = 12.0  # Blur on the dots' edges, in source texels.
STROKE = 7  # Lit areas thinner than this many source texels are paint strokes, not lights.


def lit_blobs(folder):
    """Boolean image of the areas frame 3 lights up, without the thin strokes."""
    frames = sorted(glob.glob(os.path.join(SOURCE, "Texture Animation", folder, "*.png")))
    dark = np.asarray(Image.open(frames[0]).convert("RGB"), dtype=np.int16)
    lit = np.asarray(Image.open(frames[2]).convert("RGB"), dtype=np.int16)
    rise = np.clip(lit - dark, 0, 255).max(axis=2)
    core = Image.fromarray(((rise > RISE) * 255).astype(np.uint8))
    core = core.filter(ImageFilter.MinFilter(STROKE)).filter(ImageFilter.MaxFilter(STROKE))
    grown = np.asarray(core.filter(ImageFilter.MaxFilter(25))) > 0
    return grown & (rise > RISE // 2)


def whiteness():
    """GRID x GRID image of how white (darkest channel, 0-255) the reference skin's lit frame gets."""
    frame = sorted(glob.glob(os.path.join(SOURCE, "Texture Animation", REFERENCE_SKIN, "*.png")))[2]
    white = np.asarray(Image.open(frame).convert("RGB")).min(axis=2)
    step = white.shape[0] // GRID
    return white.reshape(GRID, step, GRID, step).max(axis=(1, 3))


def hull_position_map():
    """GRID x GRID x 3 image of the mesh position under each texel; NaN off the mesh."""
    positions = np.full((GRID, GRID, 3), np.nan, dtype=np.float32)
    with open(UV_FILE) as handle:
        triangles = np.asarray(json.load(handle), dtype=np.float64)
    for triangle in triangles:
        uv = triangle[:, 3:5] * GRID
        x0, y0 = np.floor(uv.min(axis=0)).astype(int)
        x1, y1 = np.ceil(uv.max(axis=0)).astype(int)
        x0, y0, x1, y1 = max(x0, 0), max(y0, 0), min(x1, GRID - 1), min(y1, GRID - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, ay), (bx, by), (cx, cy) = uv
        area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay)
        if abs(area) < 1e-9:
            continue
        wb = ((xs - ax) * (cy - ay) - (cx - ax) * (ys - ay)) / area
        wc = ((bx - ax) * (ys - ay) - (xs - ax) * (by - ay)) / area
        wa = 1.0 - wb - wc
        inside = (wa >= -0.02) & (wb >= -0.02) & (wc >= -0.02)
        block = positions[y0:y1 + 1, x0:x1 + 1]
        block[inside] = (wa[..., None] * triangle[0, :3] + wb[..., None] * triangle[1, :3] + wc[..., None] * triangle[2, :3])[inside]
    corners = triangles[:, :, :3].reshape(-1, 3)
    return positions, corners.min(axis=0), corners.max(axis=0)


def glow_areas(positions, low, high):
    """Soft masks (GRID x GRID, 0..1) of the cockpit and of the tailfin's tip and back edge."""
    span = high - low
    place = (positions - low) / span  # 0..1 on each axis; NaN off the mesh.
    on_mesh = ~np.isnan(place[..., 0])
    along = np.where(on_mesh, 1.0 - place[..., LENGTH_AXIS] if NOSE_AT_NEGATIVE else place[..., LENGTH_AXIS], 0.0)
    height = np.where(on_mesh, place[..., UP_AXIS], 0.0)

    frame = sorted(glob.glob(os.path.join(SOURCE, "Texture Animation", REFERENCE_SKIN, "*.png")))[0]
    paint = np.asarray(Image.open(frame).convert("RGB").resize((GRID, GRID), Image.LANCZOS), dtype=np.float32)
    red = paint[..., 0] > paint[..., 1] * COCKPIT_REDNESS
    cockpit = on_mesh & red & (along > COCKPIT_FROM) & (height > 0.4)
    # The brown seam lines pass the same test; the cockpit is the one big patch.
    ys, xs = max(label(cockpit), key=lambda cells: len(cells[0]))
    cockpit = np.zeros_like(cockpit)
    cockpit[ys, xs] = True

    # The fin is thin, so work on it side-on: `back` and `up` in mesh units.
    fin = on_mesh & (height > FIN_ABOVE)
    back = (1.0 - along) * span[LENGTH_AXIS]
    up = height * span[UP_AXIS]
    fin_back, fin_up = back[fin], up[fin]
    fin_length = fin_back.max() - fin_back.min()
    fin_height = fin_up.max() - fin_up.min()

    # Top point: a round falloff.
    top = np.argmax(fin_up)
    from_tip = np.hypot(back - fin_back[top], up - fin_up[top])
    tip = np.exp(-np.square(from_tip / (fin_height * FIN_TIP_SIZE)))

    # Back edge: the rearmost point at each height, then a falloff in from it
    # that dies away toward the fin's root so it doesn't end in a corner.
    levels = np.linspace(fin_up.min(), fin_up.max(), 13)
    centres = (levels[:-1] + levels[1:]) * 0.5
    rearmost = np.asarray([fin_back[(fin_up >= a) & (fin_up <= b)].max() if ((fin_up >= a) & (fin_up <= b)).any() else np.nan
                           for a, b in zip(levels[:-1], levels[1:])])
    known = ~np.isnan(rearmost)
    from_edge = np.interp(up, centres[known], rearmost[known]) - back
    rise = np.clip((up - fin_up.min()) / fin_height, 0.0, 1.0)
    edge = np.exp(-np.square(from_edge / (fin_length * FIN_EDGE_WIDTH))) * rise * rise * (3.0 - 2.0 * rise)

    fin_glow = np.where(fin, np.maximum(tip, edge), 0.0)

    def soften(mask, close_holes):
        image = Image.fromarray(np.round(mask * 255).astype(np.uint8))
        if close_holes:
            image = image.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.MinFilter(5))
        return image.filter(ImageFilter.GaussianBlur(AREA_SOFTNESS if close_holes else 1.5))

    print("cockpit covers %.2f%% of the texture, fin glow %.2f%%" % (cockpit.mean() * 100, (fin_glow > 0.5).mean() * 100))
    # The paint test leaves pinholes in the cockpit; the fin's glow is already smooth.
    return soften(cockpit.astype(np.float32), True), soften(fin_glow, False)


def label(mask):
    """Connected components of a small boolean image, as a list of (ys, xs)."""
    remaining = {(int(y), int(x)) for y, x in zip(*np.nonzero(mask))}
    components = []
    while remaining:
        stack = [remaining.pop()]
        cells = []
        while stack:
            y, x = stack.pop()
            cells.append((y, x))
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    neighbour = (y + dy, x + dx)
                    if neighbour in remaining:
                        remaining.remove(neighbour)
                        stack.append(neighbour)
        ys, xs = zip(*cells)
        components.append((np.asarray(ys), np.asarray(xs)))
    return components


def main():
    # A light is where at least half the skins agree; the rest is repaint noise.
    votes = sum(lit_blobs(folder).astype(np.uint8) for folder in SKIN_FOLDERS)
    lights = votes >= (len(SKIN_FOLDERS) + 1) // 2
    size = lights.shape[0]
    step = size // GRID
    coarse = lights.reshape(GRID, step, GRID, step).any(axis=(1, 3))

    positions, low, high = hull_position_map()
    centre = (low + high) * 0.5
    half = (high - low) * 0.5
    white = whiteness()
    blobs = []
    for ys, xs in label(coarse):
        under = positions[ys, xs]
        under = under[~np.isnan(under[:, 0])]
        if len(under) == 0:
            print("skipped a blob off the mesh at texel", int(xs.mean()) * step, int(ys.mean()) * step)
            continue
        place = (np.median(under, axis=0) - centre) / half  # -1..1 on each axis.
        along = (1.0 - place[LENGTH_AXIS] if NOSE_AT_NEGATIVE else 1.0 + place[LENGTH_AXIS]) * 0.5
        blobs.append({"along": along, "side": place[SIDE_AXIS], "top": place[UP_AXIS] > 0.0, "ys": ys, "xs": xs,
                      "white": int(white[ys, xs].max())})

    typical = np.median([len(blob["ys"]) for blob in blobs])

    def describe(blob, what):
        print("%-9s along=%.2f side=%+.2f %-6s texel=(%4d, %4d) size=%d white=%d" % (
            what, blob["along"], blob["side"], "top" if blob["top"] else "bottom",
            int(blob["xs"].mean()) * step, int(blob["ys"].mean()) * step, len(blob["ys"]), blob["white"]))

    lines = {True: [], False: []}
    for blob in sorted(blobs, key=lambda blob: blob["along"]):
        if len(blob["ys"]) > typical * PANEL_SIZE:
            describe(blob, "panel")
        elif abs(blob["side"]) > CENTRE_SHARE:
            describe(blob, "off-line")
        elif not blob["top"] and not INCLUDE_BOTTOM:
            describe(blob, "bottom")
        elif blob["white"] < WHITE:
            describe(blob, "stroke")
        else:
            line = lines[blob["top"]]
            if line and blob["along"] - line[-1]["along"] < MERGE_GAP:
                # The other half of the previous dot.
                line[-1]["ys"] = np.concatenate([line[-1]["ys"], blob["ys"]])
                line[-1]["xs"] = np.concatenate([line[-1]["xs"], blob["xs"]])
            else:
                line.append(blob)

    typical_dot = np.median([len(blob["ys"]) for line in lines.values() for blob in line])
    for top in lines:
        for blob in lines[top]:
            if len(blob["ys"]) < typical_dot * SPECK:
                describe(blob, "speck")
        lines[top] = [blob for blob in lines[top] if len(blob["ys"]) >= typical_dot * SPECK]

    kept = []
    shape = np.zeros((GRID, GRID), dtype=bool)
    for line in lines.values():
        for number, blob in enumerate(line):
            blob["slot"] = 1.0 / len(line)
            blob["when"] = (number + 0.5) / len(line)
            shape[blob["ys"], blob["xs"]] = True
            kept.append(blob)
            describe(blob, "%d of %d" % (number + 1, len(line)))

    # Every texel takes its timing from the nearest dot, so the timing never
    # changes near a dot's edge, however far away (low mip) it is seen from.
    ys, xs = np.mgrid[0:GRID, 0:GRID]
    nearest = np.zeros((GRID, GRID), dtype=np.int32)
    best = np.full((GRID, GRID), np.inf)
    for index, blob in enumerate(kept):
        distance = (ys - blob["ys"].mean()) ** 2 + (xs - blob["xs"].mean()) ** 2
        closer = distance < best
        best[closer] = distance[closer]
        nearest[closer] = index
    when = np.asarray([blob["when"] for blob in kept])[nearest]
    slot = np.asarray([blob["slot"] for blob in kept])[nearest]
    bottom = np.asarray([0.0 if blob["top"] else 1.0 for blob in kept])[nearest]

    full = np.repeat(np.repeat(shape, step, axis=0), step, axis=1) & lights
    soft = Image.fromarray((full * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(SOFTNESS))
    soft = soft.resize((MASK_SIZE, MASK_SIZE), Image.LANCZOS)

    def flat(values):
        return Image.fromarray(np.round(values * 255).astype(np.uint8)).resize((MASK_SIZE, MASK_SIZE), Image.NEAREST)

    os.makedirs(OUT_DIR, exist_ok=True)
    for stale in glob.glob(os.path.join(OUT_DIR, "%s_RunningLights*.png" % SHIP)):
        os.remove(stale)
    path = os.path.join(OUT_DIR, "%s_RunningLights.png" % SHIP)
    Image.merge("RGBA", (soft, flat(when), flat(slot), flat(bottom))).save(path)
    print("wrote", path, "(%d dots)" % len(kept))

    cockpit, fin_back = glow_areas(positions, low, high)
    path = os.path.join(OUT_DIR, "%s_GlowAreas.png" % SHIP)
    for stale in glob.glob(path):
        os.remove(stale)
    Image.merge("RGB", (cockpit, fin_back, Image.new("L", cockpit.size, 0))).resize((MASK_SIZE, MASK_SIZE), Image.LANCZOS).save(path)
    print("wrote", path)


main()

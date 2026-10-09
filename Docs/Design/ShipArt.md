# Ship Art

The ship is one scanned mesh with several *skins*. A skin is four textures that
the material loops through, so the painted lights and windows appear to pulse.

## What's in the project

| Asset | Role |
|---|---|
| `/Game/Art/Ships/M_ShipFrameCycle` | Material: crossfades `Frame1`..`Frame4` on a loop. |
| `/Game/Art/Ships/SPCSH_Green/SM_SPCSH_Green` | The mesh (5,000 triangles). |
| `.../MI_SPCSH_Green_Green`, `_Gold`, `_Pink`, `_Purple` | One material instance per skin. |
| `.../MI_SPCSH_Green_Base` | The untouched scan texture, not animated. |
| `.../Textures/T_SPCSH_Green_<Skin>_<1-4>` | The frames, 4096 x 4096 each. |
| `.../Textures/T_SPCSH_Green_RunningLights` | Shape and timing of the centreline lights. Shared by every skin. |

Source files live outside the repo, in `D:\Source\Unreal\GatherAndHunt\Design\SPCSH_Green_V1`.
Its `Texture Animation/Green` folder (the Procreate working set) is not imported;
the four `* Color Variations` folders are.

## Material parameters

Set these on a material instance to keep them. The art preview's steppers try
values out without saving.

| Parameter | Default | Meaning |
|---|---|---|
| `CycleSeconds` | 2 | Time for one pass through the four frames. |
| `Blend` | 0.5 | Share of each frame's slot spent fading to the next. 0 cuts, 1 never holds. |
| `RunningLights` | the texture | Each dot's shape and its turn in the loop. |
| `RunningLightSeconds` | 2 | One pass of the light from the rear dot to the nose. |
| `RunningLightGap` | 0.15 | Dark beat after the last dot, as a share of the pass, before it starts again at the rear. |
| `RunningLightFade` | 0.25 | How much neighbouring dots crossfade as the light hands over. 0 switches. |
| `RunningLightStrength` | 3 | Emission of a lit dot, seen close up. |
| `RunningLightFarBoost` | 8 | Emission is scaled up with distance (in ship sizes) to at most this many times, so dots smaller than a pixel don't vanish. 1 turns it off. |
| `RunningLightColor` | warm yellow-white | The top spine's lights. |
| `RunningLightBottomColor` | cool blue-white | The bottom line's lights, so a ship that has rolled over reads differently. |
| `GlowAreas` | the texture | Red: the cockpit. Green: the tailfin's top point and back edge. |
| `CockpitGlowStrength`, `CockpitGlowColor` | 4, warm | The cockpit glows in its painted colour times this tint. |
| `CockpitPulseSeconds`, `CockpitPulseDepth` | 2, 0.6 | The cockpit dims by the depth once per period. 0 is steady. |
| `TailGlowStrength`, `TailGlowColor` | 4, green | The same for the back of the tailfin, which is steady. |
| `Roughness`, `Metallic` | 0.75, 0 | Surface. |

## Running lights

One dot is lit at a time and the light walks from the rear to the nose, on the
top spine and the bottom line together.

The texture is cut from the frames, not painted: `Tools/make_running_light_masks.py`
takes the dots that brighten between frame 1 and frame 3 in at least half the
skins, drops thin paint strokes, and finds where each dot sits on the hull through
the mesh's UVs. It keeps the dots on the centreline (8 along the top spine, 14
along the bottom) and leaves out the wing dots, the cockpit panel, seam strokes
that lighten without going white (`WHITE`) and stray specks (`SPECK`). A dot cut
in two by a seam counts once (`MERGE_GAP`). Each line is put in order from the rear and shares the pass out equally, so
the top dots are each lit a little longer and both lines reach the nose together.
`INCLUDE_BOTTOM = False` keeps the top spine only; `SOFTNESS` is the blur on the
dots' edges.

It writes `Running Lights/SPCSH_Green_RunningLights.png` in the source folder: red
is the dots' shape, green is when each dot peaks (0 to 1 through the pass), blue
is how long it is lit and alpha is 1 on the bottom line. Green, blue and alpha are
flat around each dot, so repainting red is enough to reshape or remove one. The import keeps the texture linear and
uncompressed, because green and blue are numbers.

Under the dots the base colour stays on frame 1, so the frames whitening every
light at once doesn't fight the pattern. An unlit dot shows that paint.

To regenerate the texture from the frames:

```
UnrealEditor-Cmd.exe GatherAndHunt.uproject -run=pythonscript -script="<project>\Tools\export_ship_uv.py" -EnablePlugins=PythonScriptPlugin -unattended -nullrhi
python Tools/make_running_light_masks.py
```

The second step needs numpy and Pillow, and overwrites hand edits.

## Nose and tail glows

The running lights are detail for close up: at board zoom each dot is under a
pixel. What shows a ship's heading from a distance is its two ends, following the
painted colours: the cockpit glows red and pulses, and the back of the tailfin
glows a steady green.

Close up, the hull itself glows there (`GlowAreas`, above). The mask script finds
the two areas: the cockpit is the one big patch of red-orange paint on top of the
nose, and the fin is whatever stands above the hull; it glows at its top point and along its back edge, fading smoothly away from both (`FIN_TIP_SIZE`, `FIN_EDGE_WIDTH`). It
writes `Running Lights/SPCSH_Green_GlowAreas.png`, two plain soft masks that can
be repainted.

Far away those areas are a pixel or two, so each also has a round, camera-facing
sprite (`M_ShipGlow`: unlit, additive, drawn over everything, so the hull never
hides which end is which). The sprite is invisible close up, where it would look
stuck on. It fades in as the camera pulls back and then holds a minimum on-screen
size instead of shrinking with the ship.

There are two pairs, one for each side of the ship, and a camera sees the pair on
its side (they crossfade through the edge-on view). From above: a red nose and a
green tail. From below: a darker red nose and a lighter green tail. Shades alone
are hard to tell apart, and red against green is the pair most colour-blind people
confuse, so neither distinction rests on colour only: the nose pulses and the tail
is steady, and from below the nose pulses twice as fast.

From above, two off-white strobes also blink on the wingtips (`MI_ShipGlow_Wingtip`).
The underside has none, so top and bottom differ in the number of dots (four
against two) whatever the colours. Unlike the nose and tail dots they are also
drawn close up (`bShowCloseUp`), small, as the strobe lamps themselves.

- Where, how big and the minimum size: `ShipGlows` in the art settings. `Facing`
  is the side a glow belongs to, as a direction in the mesh's axes. The
  location is in the mesh's own units; size is a fraction of the ship's length;
  a minimum of 0.02 is about 1% of the screen's width. The sprite starts to show
  when that minimum reaches about a third of its close-up size, and is full when
  the two are equal.
- Look: `MI_ShipGlow_Cockpit`, `_Tailfin`, `_NoseBelow`, `_TailBelow` and `_Wingtip` in `/Game/Art/Ships`
  (`Color`, `Strength`, `Falloff`, `PulseSeconds`, `PulseDepth`, `PulseSharpness`; depth 0 is
  steady, depth 1 goes fully out, and a high sharpness turns the swell into a short flash).
  The import script creates them once and then leaves them alone, so tune them in
  the editor. Keep the cockpit's pulse the same as `CockpitPulseSeconds`.

### Player options

How big the far lights should be and when they should come in depends on the
board's size and the player's screen, so three multipliers belong to the player,
not the art: `UGatherAndHuntPlayerOptions` (saved per user in `GameUserSettings.ini`).

| Option | Effect |
|---|---|
| Size | Multiplies every far light's minimum on-screen size. |
| Appear at distance | Multiplies the camera distance at which they fade in. Above 1 they wait until the camera is further back. |
| Blink speed | Multiplies how fast they pulse and blink. |

`UGatherAndHuntOptionsWidget` is the panel that edits them. It is on the art
preview (top right, where the effect is easiest to judge) and behind **Options**
on the main menu; any screen that builds its tree in C++ can add one. The art
settings above stay the baseline that the multipliers scale.

## Where it shows

- **Puzzles**: `AGatherAndHuntPuzzleBoardView` draws every turtle as the ship.
  The skin shows the role (player, ally, opponent) in place of the placeholder's tint.
- **Art preview**: *Art preview* on the main menu, or
  `"/Engine/Maps/Entry?game=/Script/GatherAndHunt.GatherAndHuntArtPreviewGameMode"`.
  The ship on a turntable under the board's lighting. Left / Right change skin,
  Space stops the turntable, drag orbits, the wheel zooms (far enough out to see the ship at board size).

Both read `UGatherAndHuntArtSettings` (Project Settings > Game > Gather And Hunt
Art, stored in `Config/DefaultGame.ini`): the mesh, a rotation that puts its nose
on +X, its size as a fraction of a cell, the skin list and the skin for each role.
With no mesh set, the board goes back to cones.
`GatherAndHunt.GatherAndHunt.Art.SettingsResolve` fails if a configured asset is missing.

## Importing a new drop

`Tools/import_ship_art.py` imports the mesh, frames and running-light texture and builds the material
and its instances. Edit `SOURCE` (and the names under it) at the top, then:

```
UnrealEditor-Cmd.exe GatherAndHunt.uproject -run=pythonscript -script="<project>\Tools\import_ship_art.py" -EnablePlugins=PythonScriptPlugin -unattended -nullrhi
```

Re-running replaces the mesh and textures in place. It also rebuilds
`M_ShipFrameCycle` from the script, so change the material's graph there, not in
the editor. Values set on the instances are kept.

## Not done yet

- Hazards, the chaser, objectives and the ground are still basic shapes.
- No backdrop: both screens run on the empty Entry map.

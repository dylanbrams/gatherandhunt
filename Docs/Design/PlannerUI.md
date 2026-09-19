# Turn Planner UI

The move planner from spec §16, built for the single-player puzzle runtime. It is
all C++ (no Blueprint or map assets needed), so it works in any map that runs
`AGatherAndHuntPuzzleGameMode`.

## Launching

Any map, with the GameMode and puzzle picked on the URL:

```
UnrealEditor.exe GatherAndHunt.uproject "/Engine/Maps/Entry?game=/Script/GatherAndHunt.GatherAndHuntPuzzleGameMode?Puzzle=DuoShips" -game -windowed -ResX=1600 -ResY=900
```

`?Puzzle=` is a path under `Content/BoardSetups` (the `.dat` is optional). You can
also set the map's GameMode override in World Settings and use
`AutoLoadPuzzleRelativeFilename`, or pick a file in the planner's dropdown and press
**Load** at runtime.

## Turtle roles

Every turtle has a team (the sim's `Owner`, which decides scoring) and a
controller. Together they give three roles (`EGatherAndHuntTurtleRole`):

| Role | Team | Moved by | Colour |
|---|---|---|---|
| Player | player (0) | the player's plan | green |
| Ally | player (0) | the puzzle script | blue |
| Opponent | opponent (1) | the puzzle script | magenta |

- Authoring data: `FPieceSaveData::PieceOwnerID` plus `bScriptControlled`. Use
  `UGatherAndHuntLevelEditorLibrary::GetTurtleRole` / `SetTurtleRole` rather
  than setting the fields by hand. Opponents are always scripted.
- Runtime: `UGatherAndHuntPuzzleSession::GetTurtleRole`, and `Role` on board
  snapshots and on the planner's summaries and preview tracks.
- On submit, the session merges this turn's scripted ally moves into the player
  team's plan (the player's budget covers only player turtles) and rejects player
  plans that try to move an ally.
- `AGatherAndHuntPuzzleBoardView::GetRoleColor` is the one palette for pieces,
  paths and the planner UI.

The legacy Blueprint level editor doesn't expose roles yet; its pieces load as
Player (owner 0) or Opponent (owner 1).

## Controls

| Key | Action |
|---|---|
| W / Up | Forward |
| A / Left, D / Right | Yaw left / right |
| R / F | Pitch up / down |
| S / Down | Wait |
| Backspace / Delete | Delete selected move (or the last one) |
| [ / ] | Shift selected move left / right |
| Esc | Cursor to end (new moves append) |
| Tab / Shift+Tab, 1-9 | Select turtle |
| C / Shift+C | Clear turtle / clear all |
| P | Toggle scripted turtles in the preview |
| Enter | Submit turn (or skip playback); in script mode, save the scripted turn |
| O | Toggle script mode (edit allies and opponents) |
| Ctrl+S | Save the loaded board file |
| Home | Reframe camera |
| Mouse drag / wheel | Orbit / zoom |

Move chips in the queue:

- Click a chip to select it; new moves then insert after it. Click again to go
  back to appending.
- Drag a chip to reorder. The queue and the ghost path update live as you drag.
- Right-click a chip to delete it.

Chip colour shows its cost tier (grey 1, amber 2, red 3); the offending move is
red when the plan is invalid. The turtle list shows each turtle's role as a
coloured swatch and tag.

## Authoring scripted turns

Puzzles script the ally and opponent turtles turn by turn. The planner edits
those scripts in place:

1. Load the board and press **O**. The turtle list switches to the allies and
   opponents, prefilled with the script for the current turn (if any).
2. Queue their moves as usual. The preview runs them against whatever you have
   queued for your own turtles, so you can stage the interaction.
3. Press **Enter** (*Save scripted turn N*) to store the draft as that turn's
   script. An empty draft removes the script. **Revert** discards the draft.
4. Press **O** to go back to your side and play the turn. The submit resolves
   the script you just saved, and the next turn's draft starts from its own script.
5. **Ctrl+S** writes the board, scripts included, back to the file it was loaded
   from. It refuses while a script draft is unsaved.

The status line shows whether the current turn has a script and whether the
board has unsaved changes. Scripts are keyed by the 0-based board turn. Allies and
opponents are validated with the same rules as a player's plan, each against its
own team's budget; the budget bar shows the selected turtle's team. Saving
rewrites an older board file in the current format.

## What the preview shows

The preview runs the same `turtlesim::ResolveTurn` the session uses on submit,
against the current board (with the puzzle's scripted turn, unless toggled off).
So hunters, deterministic hazards, collisions, claims and deliveries in the
preview are exact, not estimates.

- Cyan path: selected turtle. Other paths use their turtle's role colour.
- Red sphere: destroyed or blocked. Green: delivered (off the board edge). Gold: claim.
- The short bar at a path's end shows the final facing.
- The right panel summarises the player team's score change, claims, deliveries
  and losses (player turtles and allies).

## Pieces

| Class | Role |
|---|---|
| `turtlesim::ExtractTurtleTrack` (`TurtleSim/PlanPreview.h`) | Per-turtle path from a resolved turn. Pure C++, tested in the CMake suite. |
| `UGatherAndHuntTurnPlanner` | Editable plan: queues, selection, reorder, cost, validation, preview, submit, script authoring. Blueprint-callable. |
| `UGatherAndHuntPlannerWidget` | UMG widget, tree built in C++. A UMG Blueprint subclass can supply its own tree. |
| `AGatherAndHuntPuzzleBoardView` | Placeholder piece actors, playback, ghost paths, orbit camera, fallback lighting, role palette. Place one in a level to reuse it, otherwise the HUD spawns one. |
| `AGatherAndHuntPuzzleHUD` | Default HUD for the puzzle GameMode; wires the above together. |

## Board files

`ULevelSave` writes tagged UPROPERTY data followed by a hand-written trailer. The
trailer now starts with a magic number and version (`GAHS`, version 2 added
`bScriptControlled`); add new fields at the end behind a version bump. The loader
also reads:

- version 1: the same trailer without the header or `bScriptControlled`;
- original prototype boards (older trailers, pre-5.2 and pre-5.4 engine property
  tags): falls back to their tagged properties and converts `GridSize` from cells
  to world units.

`GatherAndHunt.GatherAndHunt.BoardSetups.AllFilesLoad` checks every file under
`Content/BoardSetups` loads and converts; `Automation/FormatV1_Unversioned.dat`
keeps a version 1 file in that set.

## Not done yet

- Role editing in the legacy Blueprint level editor (the C++ helpers exist).
- Multiplayer: the planner binds to a puzzle session. `TurtleGame` needs an adapter
  that feeds it the replicated board and submits through `ServerSubmitTurnPlan`.
- Real art: pieces are engine basic shapes.

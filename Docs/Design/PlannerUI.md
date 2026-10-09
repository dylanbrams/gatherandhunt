# Turn Planner UI

The move planner from spec §16, built for the single-player puzzle runtime. It is
all C++ (no Blueprint or map assets needed), so it works in any map that runs
`AGatherAndHuntPuzzleGameMode`.

## Launching

The game starts on the C++ main menu (`AGatherAndHuntMenuGameMode` on the engine's
empty `/Engine/Maps/Entry`, set as `GameDefaultMap` / `GlobalDefaultGameMode`). It
lists the board files by act and order; **Play** opens the planner, **Edit board**
opens the board editor, and **New puzzle** starts an empty board under a new file
name. **Level select** opens the campaign map (see `LevelSelect.md`). Every screen runs on the Entry map with its GameMode picked on the URL, so no
map or Blueprint assets are involved (`UGatherAndHuntFrontEnd` does the travel).

To skip the menu:

```
UnrealEditor.exe GatherAndHunt.uproject "/Engine/Maps/Entry?game=/Script/GatherAndHunt.GatherAndHuntPuzzleGameMode?Puzzle=Act1/02_Turning" -game -windowed -ResX=1600 -ResY=900
```

`?Puzzle=` is a path under `Content/BoardSetups` (the `.dat` is optional). Add
`?Edit` to open in the board editor; with `?Edit`, a file that doesn't exist yet
becomes a new empty board. You can also pick a file in the dropdown of the
planner's **AUTHORING** menu (bottom left) and press **Load** at runtime.

The authoring controls described below (script mode, the board editor, saving)
live in that menu. A level opened from the level select map only has it once the
map's first level is finished; see `LevelSelect.md`.

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

The board editor sets roles (see below).

## Controls

The plain screen is the picture move buttons (the prototype's arrow art, one per
move the puzzle allows), **Undo** and **Execute**. **Show moves** brings out the
rest: the queued moves and their reordering, the budget, the turn's numbers and
the key list. The ship list shows by itself once a puzzle has more than one ship.

**Tab** switches what the letter keys do. They start on the camera. These are
the default keys; the gamepad's, and how a player changes either, are in `Input.md`.

| Key, camera mode (default) | Action |
|---|---|
| W / S, Up / Down | Fly forward / back along the view |
| A / D, Left / Right | Strafe |
| Q / E | Down / up |
| Shift | Faster |

| Key, move mode (after Tab) | Action |
|---|---|
| W / Up | Forward |
| A / Left, D / Right | Yaw left / right |
| R / F | Pitch up / down |
| S / Down | Wait |

| Mouse | Action |
|---|---|
| Left drag | Orbit the point the camera is looking at |
| Right drag | Look around from where the camera is |
| Middle drag | Slide sideways and up / down |
| Wheel | Zoom |

| Key, either mode | Action |
|---|---|
| Backspace / Delete | Delete selected move (or the last one) |
| , / . | Shift selected move left / right |
| [ / ], 1-9 | Previous / next ship, ship by row |
| Esc | Cursor to end (new moves append); with no move selected, leave the level |
| M | Show / hide moves |
| F5 | Restart the level |
| C / Shift+C | Clear turtle / clear all |
| P | Toggle scripted turtles in the preview |
| Enter | Submit turn (or skip playback); in script mode, save the scripted turn |
| O | Toggle script mode (edit allies and opponents) |
| B | Open the board editor |
| Ctrl+S | Save the loaded board file |
| Home | Reframe camera |

Move chips in the queue:

- Click a chip to select it; new moves then insert after it. Click again to go
  back to appending.
- Drag a chip to reorder. The queue and the ghost path update live as you drag.
- Right-click a chip to delete it.

Chip colour shows its cost tier (grey 1, amber 2, red 3); the offending move is
red when the plan is invalid. The turtle list shows each turtle's role as a
coloured swatch and tag: the player's are `SHIP 1`, `SHIP 2`, numbered by row
(the 1-9 key that selects them).

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

## Puzzle rules

Each board file carries four rules (trailer version 3; older files get the
defaults in brackets):

- **Goal** [deliver]: *deliver* counts an objective once it's carried off the
  board edge; *reach* counts it on touch.
- **Player moves** [all]: which move buttons the player gets. Unavailable
  buttons are hidden and their keys ignored; the session also refuses them.
  Scripted turtles aren't limited.
- **Grass** [off]: a green field under the board.
- **Ships moved per turn** [no limit]: how many player turtles may have moves
  in one turn.

The board editor's Puzzle panel edits all four.

## Editing the board

Press **B** (or **Edit board**) to switch from planning to editing the puzzle's
starting layout, and **B** again to go back and play it. The board editor replaces
the legacy Blueprint level editor. Every edit goes to the session straight away,
so switching back plays exactly what would be saved. Entering the editor restarts
the puzzle at turn 0, and is refused while the planner has an unsaved script draft.

A yellow wire cube marks the cursor cell; the piece in that cell is the selection.
A faint outline shows the cursor's layer, with a drop line to the floor when it's
above it. Moving hazards draw their whole cycle in orange.

| Key | Action |
|---|---|
| W/A/S/D, arrows | Move the cursor (relative to the camera) |
| Q / E | Layer down / up |
| Shift + any of the above | Carry the selected piece along |
| 1-7 | Place turtle, objective, gold objective, block, hunter, patrol hazard, loop hazard |
| Delete / Backspace | Remove the selected piece |
| R / Shift+R | Rotate right / left (turtles and moving hazards) |
| T / G | Pitch a turtle up / down |
| C | Cycle a turtle's role: player, ally, opponent |
| Ctrl+Z / Ctrl+Y | Undo / redo |
| Ctrl+S | Save |
| B | Back to the planner |
| Click | Put the cursor on the clicked piece, or the clicked cell on the cursor's layer |

The panels cover the rest:

- **Puzzle**: name, file, target score, turns and objectives (0 means no
  target), act and order (the menu sorts by these), board size in cells, intro
  text. Shrinking the board is refused while a piece sits in the row being removed.
- **Cursor**: the selected piece, with role buttons for turtles and length /
  width / stride / phase steppers for moving hazards. A patrol walks *length*
  steps along its facing and back; a loop walks *width* steps to its right, then
  *length* along its facing, and back round. *Stride* is cells per step; *phase*
  is how far into its cycle the hazard starts.
- **Issues**: the level editor library's checks plus pieces sharing a cell, no
  player turtle (errors), and hazard paths that leave the board (a warning).

Save writes the file named in **File** (spaces become underscores; `.dat` is
added). Saving over a different existing puzzle takes a second press, and so
does **New** or **Main menu** with unsaved changes. You can save with errors
still listed. The message says how many, and the puzzle won't play until
they're fixed.

Deleting a turtle, or making one the player's, removes its moves from the
scripted turns.

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
| `AGatherAndHuntPuzzleBoardView` | Placeholder piece actors, playback, ghost paths, board editor overlay and picking, orbit camera, fallback lighting, role palette. Place one in a level to reuse it, otherwise the HUD spawns one. |
| `UGatherAndHuntBoardEditor` | Board editing model: cursor, place / move / rotate / delete, roles, hazard parameters, metadata, board size, validation, undo. Works in grid cells; converts to the board file's world units. Blueprint-callable. |
| `UGatherAndHuntBoardEditorWidget` | Board editor UMG widget, tree built in C++. |
| `AGatherAndHuntPuzzleHUD` | Default HUD for the puzzle GameMode; wires the above together and switches between planning and editing. |
| `UGatherAndHuntMainMenuWidget`, `AGatherAndHuntMenuHUD`, `AGatherAndHuntMenuGameMode` | Main menu and puzzle select. |
| `UGatherAndHuntFrontEnd` | Puzzle listing, filename normalising, travel to the menu / a puzzle. |

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

- Multiplayer: the planner binds to a puzzle session. `TurtleGame` needs an adapter
  that feeds it the replicated board and submits through `ServerSubmitTurnPlan`.
  The turn loop it would plug into is described in `MultiplayerTurnLoop.md`.
- Real art: turtles use the scanned ship (see `ShipArt.md`); every other piece is
  still an engine basic shape.

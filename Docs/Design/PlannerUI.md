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
| P | Toggle scripted opponent in the preview |
| Enter | Submit turn (or skip playback); in opponent mode, save the opponent turn |
| O | Toggle editing the scripted opponent |
| Ctrl+S | Save the loaded board file |
| Home | Reframe camera |
| Mouse drag / wheel | Orbit / zoom |

Click a move chip to select it; new moves then insert after it. Chip colour shows
its cost tier (grey 1, amber 2, red 3); the offending move is red when the plan is
invalid.

## Authoring scripted opponent turns

Puzzles can script the opponent's turtles (owner 1) turn by turn. The planner
edits those scripts in place:

1. Load the board and press **O**. The turtle list switches to the opponent's
   turtles, prefilled with the script for the current turn (if any).
2. Queue the opponent's moves as usual. The preview runs them against whatever
   you have queued for your own turtles, so you can stage the interaction.
3. Press **Enter** (*Save opponent turn N*) to store the draft as that turn's
   script. An empty draft removes the script. **Revert** discards the draft.
4. Press **O** to go back to your side and play the turn. The submit resolves
   the script you just saved, and the next turn's draft starts from its own script.
5. **Ctrl+S** writes the board, scripts included, back to the file it was loaded
   from. It refuses while an opponent draft is unsaved.

The status line shows whether the current turn has a script and whether the
board has unsaved changes. Scripts are keyed by the 0-based board turn and are
validated with the same rules and budget as a player's plan. Saving rewrites a
legacy board file in the current format.

## What the preview shows

The preview runs the same `turtlesim::ResolveTurn` the session uses on submit,
against the current board (with the puzzle's scripted opponent turn, unless
toggled off). So hunters, deterministic hazards, collisions, claims and deliveries
in the preview are exact, not estimates.

- Cyan path: selected turtle. Blue: other turtles on the side being edited.
  Magenta: the other side (the scripted opponent, or your queued plan while
  authoring the opponent).
- Red sphere: destroyed or blocked. Green: delivered (off the board edge). Gold: claim.
- The short bar at a path's end shows the final facing.
- The right panel summarises score change, claims, deliveries and losses.

## Pieces

| Class | Role |
|---|---|
| `turtlesim::ExtractTurtleTrack` (`TurtleSim/PlanPreview.h`) | Per-turtle path from a resolved turn. Pure C++, tested in the CMake suite. |
| `UGatherAndHuntTurnPlanner` | Editable plan: queues, selection, cost, validation, preview, submit. Blueprint-callable. |
| `UGatherAndHuntPlannerWidget` | UMG widget, tree built in C++. A UMG Blueprint subclass can supply its own tree. |
| `AGatherAndHuntPuzzleBoardView` | Placeholder piece actors, playback, ghost paths, orbit camera, fallback lighting. Place one in a level to reuse it, otherwise the HUD spawns one. |
| `AGatherAndHuntPuzzleHUD` | Default HUD for the puzzle GameMode; wires the above together. |

## Board files

`ULevelSave` files have no version header. The loader recognises the older layouts
(original prototype boards, pre-5.2 and pre-5.4 engine property tags) and falls back
to their tagged properties; legacy `GridSize` (in cells) is converted to world units.
`GatherAndHunt.GatherAndHunt.BoardSetups.AllFilesLoad` checks every file under
`Content/BoardSetups` loads and converts.

## Not done yet

- Drag-to-reorder in the queue (use `[` / `]` or the arrow buttons).
- Multiplayer: the planner binds to a puzzle session. `TurtleGame` needs an adapter
  that feeds it the replicated board and submits through `ServerSubmitTurnPlan`.
- Real art: pieces are engine basic shapes.

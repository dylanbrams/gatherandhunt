# Level Select Map

The level select is a board the ship flies over, played with the game's own
rules but a small allowance: a set of motions and a number of moves. Level
markers sit on the board's cells. A flight that touches a marker opens that
level, and winning the level adds to the allowance, which brings further markers
into range.

Nothing on the map is locked. A level is either within reach of the motions and
moves you have, or it is too far away for now.

The map is also the game's first lesson. The first level sits one cell ahead of
the ship, so the first thing a player does is queue one forward move and launch.
That replaces the old opening puzzle (`Act1/01_FirstSteps`, now removed), which
taught the same thing.

This is the framework: the rules, the data, the screen and the round trip into a
level and back. The starter map is a placeholder layout.

## Launching

**Level select** on the main menu, or:

```
UnrealEditor.exe GatherAndHunt.uproject "/Engine/Maps/Entry?game=/Script/GatherAndHunt.GatherAndHuntCampaignGameMode" -game -windowed -ResX=1600 -ResY=900
```

## How it plays

- The ship starts every flight from its starting cell on the map board.
- You queue up to *moves per flight* moves, using only the motions unlocked so
  far, and launch. Every move counts as one; the puzzle cost tiers and the
  20-point budget don't apply here.
- The level you enter is the **last marker the ship touched**, provided the ship
  survived the flight. Flying through one marker to reach another enters the
  second. A flight that reaches nothing, or loses the ship, puts it back at the
  start.
- Winning a level opened from the map records it as finished and adds its reward
  to the allowance. The planner then offers **Enter: back to the map**. A
  finished level can be flown to and replayed; it earns nothing the second time.
- Blocks, hazards and chasers on the map behave as they do in a puzzle.

### The screen

The plain screen keeps the detail out of the way: a big picture button per motion
earned (the prototype's arrow art), Undo, Clear and Launch, and the move count.

- Levels are marked **I, II, III...** in the order they are listed. A finished
  level's ball is see-through and its label dimmed.
- **Hold Ctrl or Alt** to put the levels' names on the markers and bring up the
  list of levels (name, cell, done / in reach / too far, what it earns).
- **Show flight plan** lists the queued moves. The ghost path on the board shows
  them either way.
- **Tab** switches what the letter keys do. By default they fly the camera;
  after Tab they queue moves (W forward, A/D turn, R/F nose up/down, S wait).

The camera is the same free camera as in a puzzle; see `PlannerUI.md`.
Backspace undoes, C clears, [ and ] pick a ship if the map has several, Enter
launches or skips the flight, Home reframes, Esc goes to the main menu. On a
gamepad the D-pad queues moves, A launches and holding View shows the names;
`Input.md` has the full list.

## Authoring

Two files under `Content/BoardSetups/Campaign`:

**`Map.dat`** is an ordinary board file: the map's size, its blocks and hazards,
and the ship at its starting cell and facing. It is listed in the main menu
(Act 0), so **Edit board** opens it in the board editor. Balls placed on it are
ignored; markers come from the JSON. `Tools/make_campaign_map.py` wrote the
starter map and won't overwrite an existing one.

**`Campaign.json`** is the rest:

```json
{
	"MapBoard": "Campaign/Map",
	"StartingMoves": 1,
	"StartingMotions": ["Forward"],
	"Nodes": [
		{
			"Id": "act1-02",
			"Title": "Turning",
			"Cell": {"X": 1, "Y": 3, "Z": 0},
			"Puzzle": "Act1/02_Turning",
			"Reward": {"Moves": 3, "Motions": ["YawLeft", "YawRight"]}
		}
	]
}
```

- `Id` is what the save file stores; keep it stable. Letters, digits, `_`, `-`.
- `Cell` is a grid cell on the map board. `Puzzle` is a board file.
- Motions are `Forward`, `YawLeft`, `YawRight`, `PitchUp`, `PitchDown`, `Wait`.
- List the nodes in the order they are meant to be played. The
  `ShippedCampaignCompletable` test plays them in that order and fails if one is
  out of range when its turn comes.
- The first node is the first level: finishing it is what brings out the
  planner's authoring menu (below). Keep it one forward move from the ship.

The allowance is the starting values plus the rewards of every finished level,
worked out from this file each time. Changing a reward therefore applies to
existing saves.

Progress is the `Campaign` save slot (`Saved/SaveGames/Campaign.sav`): the ids
of the finished levels. **Reset progress** on the map deletes it.

### The starter map

Nine by seven cells, three high. The ship faces east, at the first level. The
allowance starts at one move, forward only.

| Level | Cell | Shortest flight | Moves needed | Earns | Allowance after |
|---|---|---|---|---|---|
| Turning | (1,3,0) | F | 1 | +3 moves, yaw | 4 |
| Obstacle | (1,5,0) | F L FF | 4 | +3 moves | 7 |
| Gathering | (4,5,0) | FFFF L FF | 7 | +2 moves | 9 |
| Up High | (6,1,0) | FFFFFF R FF | 9 | +1 move, pitch | 10 |
| The Chaser | (6,3,2) | FFFFFF U FF | 9 | +2 moves, wait | 12 |
| First (Act 2) | (8,4,1) | FFFFFFFF L F U F | 12 | +2 moves | 14 |
| Duo Ships | (8,1,2) | FFFFFFFF R FF U FF | 14 | +2 moves | 16 |

A motion is earned from the level that teaches it, so each of those levels has
to be reachable without it.

## Authoring tools in a level

The planner keeps everything that changes a puzzle rather than plays it in an
**AUTHORING** menu at the bottom left: script mode (O), the board editor (B),
saving the file (Ctrl+S) and opening another board. It starts collapsed.

In a level opened from the map, the menu and its shortcuts are absent until the
map's first level has been finished; they appear the moment it is won. A puzzle
opened from the main menu always has them, since the main menu is itself the
authoring front end (`AGatherAndHuntPuzzleGameMode::AreAuthoringToolsAvailable`).

## Pieces

| Class | Role |
|---|---|
| `UGatherAndHuntCampaign` | The definition and the progress. Works out the allowance, builds the map as a puzzle (`BuildMapLevelSave`), decides which level a flight enters (`FindEnteredNode`), finds the markers in range (`FindReachableNodes`), checks the authoring (`Validate`). |
| `UGatherAndHuntCampaignSave` | The save slot: finished level ids. |
| `AGatherAndHuntCampaignGameMode` | A puzzle GameMode whose puzzle is the built map. |
| `AGatherAndHuntCampaignHUD`, `UGatherAndHuntCampaignMapWidget` | The map screen. |
| `UGatherAndHuntPuzzleSession::SetMaxMovesPerTurn` | The move cap, a puzzle rule the map sets. It isn't stored in board files. |
| `AGatherAndHuntPuzzleGameMode::CampaignNodeId` | Set by `?CampaignNode=<id>` when the map opens a puzzle; a win is recorded against it. |
| `AGatherAndHuntPuzzleBoardView::ObjectiveLabels`, `FadedObjectiveIds` | Label the markers in place of the balls' `+N`, and fade the finished ones. |
| `GatherAndHuntUI::MakeMoveButton` (`GatherAndHuntWidgetBuilder.h`) | The picture move buttons, shared with the planner. |
| `FGatherAndHuntCameraInput` | The free camera's mouse and key handling, shared with the planner. |

The map reuses the puzzle session, the turn planner, the resolver preview and the
board view, so the ghost path on the map is the same exact preview as in a puzzle.
The markers are gold objectives on a reach-rules board.

## Decisions

Settled by the designer (2026-10-09): every flight starts from the ship's
starting cell, and no level is ever locked, only out of reach.

Made while building the framework and not yet reviewed:

- **Moves are a plain count**, not the puzzle's tiered cost.
- **The last marker touched wins**, so levels can sit in a line.

## Not done yet

- `FindReachableNodes` is exact for a map of static blocks. With moving hazards
  or chasers on the map it can call a marker in range when the timing doesn't
  work out, since it searches one move at a time.
- It searches for the map's first player ship only.
- No way back to the map from a level's defeat other than **Back to map**.
- Markers are the placeholder balls.
- The main menu's own authoring entries (Edit board, New puzzle) aren't held
  back by progress.
- The remaining Act 1 files keep their numbers (`02_Turning` is now the first).

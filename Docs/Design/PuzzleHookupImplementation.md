# Puzzle Hookup: C++ Instead Of Blueprints

The earliest version of this document planned to hook the C++ puzzle runtime up
through new and existing Blueprints. That plan is dropped: Blueprints are hard to
read, diff and maintain, so gameplay, UI and flow live in C++ and Blueprints are
kept to a minimum (at most thin UMG or data subclasses for styling, never logic).

## What replaced what

| Legacy content | Replacement (C++) |
|---|---|
| `GameModes/BP_GatherAndHuntGameInstance` | None needed: the puzzle file travels on the URL (`?Puzzle=`). `GameInstanceClass` is no longer set. |
| `GameModes/MainMenuGameMode`, `BP_InitializeMenu`, `UI/BP_MainMenuController`, `UI/MainMenuUI`, `UI/MainMenuMultUIManager`, `UI/MultiUIManager`, `UI/BaseMultiUIInterface` | `AGatherAndHuntMenuGameMode`, `AGatherAndHuntMenuHUD`, `UGatherAndHuntMainMenuWidget` |
| `UI/BP_LevelSelectController`, `UI/LevelSelectUI`, `UI/LevelSelectTray`, `UI/LevelSelectionButton` | The main menu's puzzle list (`UGatherAndHuntFrontEnd::ListPuzzles`) |
| `GameModes/GatherHuntPlayGameMode`, `BP_InitializePlay`, `Levels/BP_PrimaryGameLevel` | `AGatherAndHuntPuzzleGameMode` + `AGatherAndHuntPuzzleHUD` |
| `GameBoardManagerObjects/*` (board manager, field, tiles, score, line spawner) | `UGatherAndHuntPuzzleSession` (rules via TurtleSim) + `AGatherAndHuntPuzzleBoardView` (visuals) |
| `GameBoardManagerObjects/BP_PieceLoadSaveManager`, `Save/BP_LevelSave`, `Save/BP_SavedPiece`, `GameBoardPieceAdder`, `UI/LevelEditingHUD` | `UGatherAndHuntBoardEditor` + `UGatherAndHuntBoardEditorWidget` (board files via `ULevelSave`) |
| `GamePieces/*` (pieces, actions, movement styles, collision interface) | TurtleSim resolver; `AGatherAndHuntPuzzlePieceActor` + `UGatherAndHuntPuzzlePlaybackComponent` for visuals |
| `UI/BP_HUDController`, `UI/Actions/*`, `UI/BP_AvailableActionTray`, `UI/BP_CurrentActionQueue`, `UI/BP_ScoreWidget`, `UI/BP_CustomSpectatorCamera` | `UGatherAndHuntTurnPlanner` + `UGatherAndHuntPlannerWidget`; orbit camera on the board view |
| `UI/BP_OptionsController`, `UI/OptionsUI` | Nothing yet (there were no options that affected the game). |
| `Rotations/*`, `Test/*`, `xxxUIBlueprints/*` | Prototype demos, not needed. |
| `Levels/MainMenu`, `PrimaryGameLevel`, `PrimaryGameLevelMyTerrain`, `EditingGameLevel` | `/Engine/Maps/Entry` with the GameMode on the URL. A scenery-only level can come back later for art; it must not contain gameplay Blueprints. |

The UI input assets under `UI/Input/` (Enhanced Input actions and mapping context)
aren't used either; the C++ widgets read keys directly.

The Blueprint-only C++ helpers `UMyArrayUtilities` and `UMyFileUtilities` went
with them. `USavedPieceCPP` stays, because old board files serialize a
`SavedPieces` array of it.

## Retired (2026-09-25)

Everything in the table above has been deleted, along with the four maps and
their `__ExternalActors__` / `__ExternalObjects__` folders. Git history has
them if anything needs looking up. What's left in `Content/` is puzzle data
(`BoardSetups/`) and art (`GFXResources/`, `PolygonSciFiSpace/`, `Megascans/`,
`MSPresets/`, `Textures/`), with no Blueprints or maps.

## How the old level system worked, and the import

- A level was a `.dat` file under `Content/BoardSetups`: `BP_LevelSave` /
  `ULevelSave` serialized to disk, holding pieces (`FPieceSaveData`) plus
  metadata. `LevelSelectTray` listed that folder; `BP_GameBoardManager` loaded
  the chosen file and spawned `BP_BasePiece` subclasses on a 10×10×10 grid at
  100-unit spacing (`BoardXYZSize` / `BoardSpacing` on the placed manager).
- Pieces could also be placed by hand in a map. `EditingGameLevel`'s pieces are
  exactly `DuoShips`. `PrimaryGameLevelMyTerrain`'s layout existed only in the map.

Every distinct layout was first imported into `Content/BoardSetups/Act1/` in the
current file format, as below. On 2026-09-25 the one-ship imports (The Beginning,
Obstacle, Two Prizes) were replaced by the Act 1 tutorial in `SinglePlayerActs.md`,
which covers the same lessons. First and DuoShips moved to `Act2/01_First` and
`Act2/02_DuoShips` as one-ship-per-turn reach puzzles with no turn limit.

| File | Source | Notes |
|---|---|---|
| `01_TheBeginning` | `archive/0010.dat` | 1 objective in 1 turn; verified winnable (11 forwards, cost 17). |
| `02_Obstacle` | `archive/0100LevelSave.dat` | Was unnamed. Named "Obstacle" because it's the layout with a block behind the objective. No targets. |
| `03_TwoPrizes` | Pieces placed in `PrimaryGameLevelMyTerrain` | Name is new. No targets. |
| `04_First` | `archive/firstLevel.dat` | Original targets kept (2 objectives, 1 turn); not verified winnable under the current rules. |
| `05_DuoShips` | `DuoShips.dat` / `EditingGameLevel` | Original targets kept (2 objectives, 2 turns); not verified. |

Not imported: `archive/0020.dat` ("Obstacle"), which has the same layout as
`0010` but a target of 2 objectives with only 1 on the board.

Objectives' owner (legacy -1 or 9) was normalised to 0. DuoShips' floating hazard
at (4,4,3) keeps its legacy `BP_ForwardTwoDown` movement, which TurtleSim
doesn't model, so it plays as a static block.

The original files are kept, unmodified, in `BoardSetups/Automation/Legacy/` as
fixtures for the old-format loader (`BoardSetups.AllFilesLoad`); the menu hides
`Automation/`.

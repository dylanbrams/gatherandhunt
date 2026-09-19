# Puzzle Hookup Implementation

This is the concrete hookup list for getting `GatherAndHunt` playable and testable with the new puzzle runtime.

It assumes the current C++ runtime pieces are already in place:

- `UGatherAndHuntPuzzleSession`
- `AGatherAndHuntPuzzleGameMode`
- `AGatherAndHuntPuzzleGameState`
- `UGatherAndHuntPuzzleBlueprintLibrary`
- `UGatherAndHuntLevelEditorLibrary`

The main goal here is to connect those systems to Unreal content in a way that supports:

- regular player-facing single-player puzzle flow
- dedicated test/playground levels
- a clear lobby/menu entry point
- a gameplay map that actually hosts puzzle sessions

## Current Content To Reuse

Existing assets that are likely the right starting points:

- Level: `Content/Levels/MainMenu.umap`
- Level: `Content/Levels/PrimaryGameLevel.umap`
- Level: `Content/Levels/EditingGameLevel.umap`
- GameInstance: `Content/GameModes/BP_GatherAndHuntGameInstance.uasset`
- Menu/Game bootstrap: `Content/GameModes/BP_InitializeMenu.uasset`
- Play bootstrap: `Content/GameModes/BP_InitializePlay.uasset`
- Legacy play GameMode: `Content/GameModes/GatherHuntPlayGameMode.uasset`
- Legacy menu GameMode: `Content/GameModes/MainMenuGameMode.uasset`
- Board manager: `Content/GameBoardManagerObjects/BP_GameBoardManager.uasset`
- Save/load manager: `Content/GameBoardManagerObjects/BP_PieceLoadSaveManager.uasset`
- HUD controller: `Content/UI/BP_HUDController.uasset`
- Level select UI: `Content/UI/BP_LevelSelectController.uasset`
- Main menu UI: `Content/UI/BP_MainMenuController.uasset`
- Level editor HUD: `Content/UI/LevelEditingHUD.uasset`

These do not need to survive unchanged. They are the most likely hook points.

## Levels To Create Or Standardize

You should end up with these levels, even if some start as renamed copies of existing maps.

### Player-facing levels

- `L_MainMenu`
  Use `Content/Levels/MainMenu.umap` or replace it in place.
  Purpose: title screen, entry point, route to lobby, route to puzzle select, route to editor/test levels.

- `L_Lobby`
  Create this.
  Purpose: lightweight session staging level for future multiplayer and local mode selection.
  Even if multiplayer is not finished, this should exist as the place where mode/puzzle selection happens cleanly.

- `L_PuzzleGameplay`
  Create this or repurpose `Content/Levels/PrimaryGameLevel.umap`.
  Purpose: the main single-player runtime map using `AGatherAndHuntPuzzleGameMode`.
  This should be the canonical puzzle play scene.

- `L_PuzzleSelect`
  Create this or repurpose part of `MainMenu`.
  Purpose: choose authored puzzle files and route into `L_PuzzleGameplay`.

### Test / developer levels

- `L_TestPuzzleGameplay`
  Create this.
  Purpose: fast developer test map with puzzle auto-load turned on and minimal menu flow.

- `L_TestPuzzleRuntime`
  Create this if you want a stripped scene separate from the prettier play map.
  Purpose: verify piece spawning, turn submission, hazard timing, and scripted opponent behavior with minimal art noise.

- `L_LevelEditor`
  Use `Content/Levels/EditingGameLevel.umap`.
  Purpose: author puzzles and preview/save them through the new C++ backend.

### Future multiplayer levels

- `L_MultiplayerLobby`
  This can initially be the same physical map as `L_Lobby`.
  Purpose: explicit place to separate multiplayer setup from puzzle mode.

- `L_MultiplayerGameplay`
  This can initially be the same physical map as `PrimaryGameLevel` if needed.
  Purpose: eventual `TurtleGame` match runtime.

The important point is not the exact names. The important point is separating:

- menu
- lobby / selection
- single-player puzzle gameplay
- test gameplay
- editing
- future multiplayer gameplay

## Blueprints To Create

These are the highest-value new Blueprint classes/widgets to add.

### GameMode / GameState wrappers

- `BP_PuzzleGameMode`
  Parent: `AGatherAndHuntPuzzleGameMode`
  Use on `L_PuzzleGameplay` and `L_TestPuzzleGameplay`.
  Responsibilities:
  - set `AutoLoadPuzzleRelativeFilename` for test maps
  - expose map-specific defaults
  - call into menu/UI flow when puzzle ends

- `BP_PuzzleGameState`
  Parent: `AGatherAndHuntPuzzleGameState`
  Optional but useful if the UI wants a Blueprint-native state object.

### Runtime orchestrators

- `BP_PuzzleRuntimeController`
  Create this.
  Recommended role: level actor or subsystem-facing actor placed in `L_PuzzleGameplay`.
  Responsibilities:
  - fetch `UGatherAndHuntPuzzleSession` via `UGatherAndHuntPuzzleBlueprintLibrary`
  - trigger initial puzzle load if map-specific flow needs it
  - notify board manager / HUD when board snapshots change
  - handle restart, next puzzle, fail/retry actions

- `BP_PuzzleBoardPresenter`
  Create this if you want to keep spawn/update logic separate from the generic board manager.
  Responsibilities:
  - read current `FGatherAndHuntPuzzleBoardSnapshot`
  - spawn/update piece actors
  - apply `FGatherAndHuntPuzzleTurnResultView` events if doing playback

### Menu / lobby / selection

- `BP_LobbyController`
  Create this.
  Responsibilities:
  - choose mode: puzzle / test / multiplayer
  - choose puzzle pack or explicit puzzle file
  - travel to gameplay map with the selected puzzle

- `BP_PuzzleSelectController`
  Create this or extend `BP_LevelSelectController`.
  Responsibilities:
  - enumerate available `.dat` puzzle files
  - show puzzle metadata
  - launch selected puzzle

- `BP_PuzzleSelectEntry`
  Widget.
  Responsibilities:
  - display one puzzle row/card
  - show act/order/title/par summary

### Gameplay UI

- `BP_PuzzleHUDController`
  Create this or refactor `BP_HUDController`.
  Responsibilities:
  - build a `FGatherAndHuntPuzzleTurnPlan`
  - call `AddTurtlePlanMove`
  - call `SubmitTurnPlanFromWorld`
  - display validation failures
  - display turn number, score, target score, target turns, intro text

- `WBP_PuzzleHUD`
  Widget.
  Responsibilities:
  - action buttons
  - turtle selection
  - current queued moves
  - score and target display
  - restart / retry / back-to-lobby

- `WBP_PuzzleResults`
  Widget.
  Responsibilities:
  - show victory / defeat
  - retry
  - next puzzle
  - return to lobby

### Editor / authoring

- `BP_PuzzleEditorController`
  Create this for `L_LevelEditor`.
  Responsibilities:
  - call `UGatherAndHuntLevelEditorLibrary`
  - load/save puzzle files
  - update metadata fields
  - preview hazard paths

- `WBP_PuzzleEditorPanel`
  Widget.
  Responsibilities:
  - metadata editing
  - save/load
  - piece list
  - scripted opponent turn editing

## Existing Blueprints To Hook Or Refactor

These are the likely existing assets to wire into the new runtime rather than replacing blindly.

- `BP_GatherAndHuntGameInstance`
  Hook for selected puzzle filename, selected mode, and travel state.

- `BP_InitializeMenu`
  Hook into `L_MainMenu` / `L_Lobby` startup flow.

- `BP_InitializePlay`
  Hook into `L_PuzzleGameplay` startup if you want existing boot logic to remain in place.

- `GatherHuntPlayGameMode`
  Decide whether to retire this or make it a Blueprint child of `AGatherAndHuntPuzzleGameMode`.
  Do not leave two competing gameplay runtime paths alive longer than necessary.

- `BP_GameBoardManager`
  Likely reuse as the scene-level piece/board presenter.
  It should stop owning authoritative gameplay logic.
  It should read from puzzle session snapshots / turn results instead.

- `BP_PieceLoadSaveManager`
  Hook this into `UGatherAndHuntLevelEditorLibrary` and stop relying on any stale custom serialization logic.

- `BP_HUDController`
  Either split it into puzzle-specific and legacy variants, or retarget it onto `UGatherAndHuntPuzzleBlueprintLibrary`.

- `BP_LevelSelectController`
  Strong candidate to become the puzzle select screen with file-backed puzzles instead of old assumptions.

## Hookup Order

Use this order. It keeps the integration testable.

1. Create `BP_PuzzleGameMode` and assign it to `L_TestPuzzleGameplay`.
2. Create `BP_PuzzleRuntimeController`.
3. In that controller, fetch the puzzle session from the GameMode and confirm a board snapshot can be read.
4. Hook `BP_GameBoardManager` or a new `BP_PuzzleBoardPresenter` to spawn/update the board from `GetCurrentBoardSnapshot`.
5. Create `BP_PuzzleHUDController` and submit a hardcoded one-turn plan.
6. Confirm turn submission updates the board, score, and outcome.
7. Add puzzle file selection from `L_Lobby` / `L_PuzzleSelect`.
8. Hook `BP_GatherAndHuntGameInstance` to carry the selected puzzle name across travel.
9. Wire restart / retry / next puzzle.
10. Only then clean up or retire legacy play-flow Blueprints.

## Minimum Play/Test Setup

If you want the smallest viable setup first, do only this:

- `L_TestPuzzleGameplay`
- `BP_PuzzleGameMode`
- `BP_PuzzleRuntimeController`
- `BP_PuzzleHUDController`
- reuse `BP_GameBoardManager`
- one hardcoded puzzle filename

That is enough to verify:

- puzzle file loads
- board appears
- hazards move
- scripted opponent turns resolve
- player plan submission works
- victory/defeat can be reached

## Full Regular Setup

For a proper player-facing vertical slice, build:

- `L_MainMenu`
- `L_Lobby`
- `L_PuzzleSelect`
- `L_PuzzleGameplay`
- `BP_GatherAndHuntGameInstance` hookup for selected puzzle
- `BP_PuzzleSelectController`
- `BP_PuzzleHUDController`
- `WBP_PuzzleResults`

That gives you:

- menu -> lobby -> puzzle select -> gameplay
- retry / next / back
- a usable single-player shell

## Suggested Asset Decisions

These are the practical choices I would make now:

- Keep `Content/Levels/EditingGameLevel.umap` as the authoring level.
- Repurpose `Content/Levels/PrimaryGameLevel.umap` into puzzle gameplay if it already has the right camera/board framing.
- Keep `Content/Levels/MainMenu.umap` as the menu shell.
- Create a new `L_Lobby` instead of overloading `MainMenu`.
- Create a new `L_TestPuzzleGameplay` instead of abusing the real play map for testing.
- Treat `GatherHuntPlayGameMode` as legacy unless you explicitly reparent it to the new puzzle GameMode path.

## What Still Needs Real Work After Hookup

Hooking it up will make it playable, but these are still separate work items:

- proper actor playback from `FGatherAndHuntPuzzleTurnResultView`
- prettier board-to-actor sync and interpolation
- scripted opponent authoring UI inside the editor
- puzzle metadata UI polish
- puzzle progression/save system
- multiplayer lobby/gameplay hookup on the `TurtleGame` side

## Are You Ready To Hook It Up?

Yes, for the single-player puzzle/runtime path.

You now have enough C++ infrastructure to hook the content up without inventing missing core systems:

- puzzle save/load is real
- validation is real
- deterministic hazard runtime is real
- scripted opponent turns are real
- local turn resolution is real
- Blueprint access points are real
- automated tests are covering the critical core path

What you are not fully ready for yet is a polished end-to-end product or the final multiplayer shell.

The practical answer is:

- **Yes:** ready to hook up single-player play and test levels now.
- **Yes:** ready to hook the legacy editor level into the new save/runtime path now.
- **Partly:** ready for a lobby/menu shell now, but it will still be mostly routing/UI work.
- **Not fully:** ready for final multiplayer gameplay/lobby integration. That still has remaining runtime/UI/playback work outside this puzzle document.

If you want the best next move after this document, it is:

1. build `L_TestPuzzleGameplay` first
2. hook `BP_PuzzleGameMode` + `BP_PuzzleRuntimeController` + board presenter
3. prove one puzzle plays cleanly
4. then build `L_Lobby` and `L_PuzzleGameplay`

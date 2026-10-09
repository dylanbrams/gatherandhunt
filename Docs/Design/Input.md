# Input

The board screens (the planner and the level select map) take keyboard and
gamepad through Enhanced Input. There are no input assets: `UGatherAndHuntInput`
makes an Input Action per `EGatherAndHuntInputAction` and four mapping contexts
in C++, from one bindings table (`GetDefaultBindings` in `GatherAndHuntInput.cpp`).

The screens never read keys. They listen for presses (`OnPressed`) and poll the
held actions and axes (`IsHeld`, `GetAxis`), so a different key is only a
different row in the table. The mouse (buttons, camera drags, chips) is still
handled by the widgets.

## Contexts

| Context | On when | Holds |
|---|---|---|
| `Common` | always | the gamepad, and keys that mean one thing |
| `CameraKeys` | by default | W/A/S/D, Q/E and the arrows as the camera |
| `ShipKeys` | after Tab | the same keys as the ship's moves |
| `Chords` | always | Ctrl+S, Shift+C |

Tab (`ToggleKeyMode`) swaps `CameraKeys` and `ShipKeys`. The gamepad needs no
such switch, since the D-pad and the sticks don't overlap.

## Default bindings

| Action | Keyboard | Gamepad |
|---|---|---|
| Forward / turn left / turn right | W, A, D or arrows (ship keys) | D-pad up / left / right |
| Nose up / nose down | R / F (ship keys) | Y / X |
| Wait | S or Down (ship keys) | D-pad down |
| Undo | Backspace, Delete | B |
| Execute / launch, skip playback | Enter (Space also skips) | A |
| Back (deselect a move, then leave) | Esc | Menu / Start |
| Restart level | F5 | View / Select (in a level) |
| Next / previous ship | ] / [ | RB / LB |
| Pick ship by row | 1-9 | |
| Move a queued move earlier / later | , / . | |
| Clear ship / clear all | C / Shift+C | |
| Show or hide moves (flight plan) | M | left stick click |
| Show level names (held, on the map) | Ctrl or Alt | View / Select |
| Fly camera | W/S, A/D, Q/E or arrows (camera keys) | left stick, LT down, RT up |
| Turn camera | mouse drag | right stick |
| Fly faster (held) | Shift | |
| Reframe camera | Home | right stick click |
| Switch keys: camera / ship | Tab | |
| Authoring: script mode, board editor, save, scripted preview | O, B, Ctrl+S, P | |

A Steam Deck shows up as a gamepad, so the gamepad column is the Deck's. Its
touchscreen and trackpads act as the mouse for everything else.

## Focus

Enhanced Input only hears keys the UI doesn't take. The board screens therefore
run in Game-and-UI input mode with the keyboard on the game viewport: the widgets
aren't focused and their buttons can't take focus. A click on empty space hands
the keyboard back to the game after a dropdown or text box has had it.

The board editor still reads keys itself. While it is up the HUD focuses it and
turns the input actions off (`UGatherAndHuntInput::SetEnabled`).

## Custom controls

A player's bindings are overrides of table rows, stored with the other player
options (`UGatherAndHuntPlayerOptions::KeyOverrides`, in `GameUserSettings.ini`):

```ini
[/Script/GatherAndHunt.GatherAndHuntPlayerOptions]
+KeyOverrides=(Id="Ship.Forward",Key=I)
+KeyOverrides=(Id="Pad.Submit",Key=Gamepad_FaceButton_Top)
```

`Id` is the row's name in the table; an empty key unbinds the row.
`UGatherAndHuntInput::GetBindings` applies them and `RebuildMappings` puts a
change into effect on a live screen.

What is missing is the screen to edit them: a list of actions
(`UGatherAndHuntInput::GetActionName`), a button per row that waits for the next
key or gamepad button, a check for a key already in use, and a reset.

## Not done yet

- The controls screen above.
- Gamepad on the main menu is only D-pad up/down, A to play and Y for the level
  select. The options panel, the authoring menu and the board editor need the
  mouse (or the Deck's touchscreen).
- No on-screen button hints, so nothing tells a gamepad player which button does
  what, and the hints that exist name keys.
- The gamepad bindings have not been tried on a controller or a Deck.

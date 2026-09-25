# Multiplayer Turn Loop

How `TurtleGame` gets a turn from two plans to two animated clients and back.
Spec: `Docs/LLM/GatherAndHuntDesign.md` §13–14.

## The loop

1. **Planning / AwaitingPlans** — clients call `ServerSubmitTurnPlan`.
   `ATurtleGameMode::HandleSubmittedPlan` re-validates every plan against the
   authoritative board and forces `Plan.Player` to the sender's slot, so a
   client can't move the other side's turtles. Rejected plans are dropped and
   the player stays in Planning.
2. **Resolving** — `ResolveTurn` runs on the server board. The result is
   serialized once and sent to every slot via `ClientReceiveTickLog`; the
   post-turn board also goes out on the replicated `MatchState`.
3. **Animating** — each client walks the tick log, then acknowledges.
4. **Planning again, or Ended** — once both slots acknowledge.

## Acknowledgement is the client's promise it finished animating

The server holds the next turn until both slots call
`ServerAcknowledgePlayback(TurnNumber)`, so whoever sends it must actually be
done. Exactly one thing acknowledges a turn:

- an `ABoardPresenter` is in the world: it registers as a *playback observer* on
  the GameState in `BeginPlay`, and `UTurtlePlaybackController::FinishPlayback`
  acknowledges when the last tick has played;
- nobody is observing (a headless test, a dedicated-server-side client, a
  spectator with no presenter): `ATurtlePlayerController::ClientReceiveTickLog`
  acknowledges on arrival, because there is nothing to wait for.

Acknowledging in both places would let the server start the next turn while the
client was still animating the last one, which is what `HasPlaybackObserver`
exists to prevent.

`HandlePlaybackAck` ignores an acknowledgement whose `TurnNumber` isn't the turn
just resolved, so a late one from a previous turn can't advance the match.

### The playback timeout is a safety net, not the normal path

The Animating phase's deadline is `LastTurnTickCount * EstimatedSecondsPerTick +
PlaybackTimeoutSeconds` — the estimated animation plus a grace period. Keep
`EstimatedSecondsPerTick` in step with `UTurtlePlaybackController::SecondsPerTick`,
or long turns will time out mid-animation and cut playback short.

## Animating forward, not backward

Playback needs the board as it stood *before* the turn. The post-turn board
arrives twice — on the replicated `MatchState` and in the tick log RPC — in
either order, so `ATurtleGameState` keeps the outgoing board itself: whichever
lands first pushes the previous board aside, and the second, carrying the same
`TurnNumber`, leaves it alone. `GetPlaybackStartBoardState()` hands that board
back only when it really is the result's predecessor.

When it isn't — a mid-match join, or a full-state resync — the presenter calls
`SkipPlayback`, snapping to the final state and acknowledging. Feeding the
post-turn board in as the start state instead would teleport every piece to
where it ends up and then animate from there.

## Ending the match

`EvaluateMatchOutcome` is a pure verdict on a board: whether it ended, why, and
who won. It ends on any of

- a side with no turtles left on the board (`TeamOutOfPlay` — destroyed or
  delivered, since neither can act again),
- every objective claimed with nobody still carrying (`ObjectivesGone`),
- `TurnNumber >= MaxTurns` (`TurnLimit`).

The winner follows the spec's ladder: score, then turtles delivered, then
turtles surviving, then a draw (`WinnerSlot == -1`). A delivered turtle is still
alive, so it counts on both of the last two rungs.

The verdict is recorded when the turn resolves but applied only once both
clients acknowledge playback — entering `Ended` immediately would cut short the
animation of the very turn that ended the match. `WinnerSlot` and `EndReason`
on the replicated `MatchState` are valid once the phase is `Ended`.

## Not done yet

- The planner (`UGatherAndHuntTurnPlanner`) still binds to a single-player
  puzzle session; it needs an adapter feeding it the replicated board and
  submitting through `ServerSubmitTurnPlan`.
- Surrender and disconnect endings (spec §13) aren't implemented; a player
  leaving just frees the slot and the phase timeouts carry the match.
- No automated coverage of the two-client loop end to end — the tests cover the
  serializer, the GameState's caching rules and the outcome verdict, but the
  phase machine itself is only exercised by hand.

# Single-Player Puzzle Acts

This lays out a three-act structure for the planned post-v1 single-player mode in `GatherAndHunt`.

The goal is to teach the simulation cleanly in Act 1, force combination play in Act 2, and make simultaneous multi-turtle timing the real skill test in Act 3.

## The designer's progression (2026-09-25)

This is the plan. It follows the original prototype, whose quick difficulty curve
and sense of progression are what to keep. The sections after it are an idea
bank for puzzles within this structure, not a competing plan.

- **Act 1: one ship.** A six-puzzle tutorial that hands out buttons one lesson
  at a time, then several puzzles of increasing complexity, still with one ship.
- **Act 2: several ships, one moving per turn** (`MaxTurtlesMovedPerTurn = 1`).
- **Act 3: several ships moving at the same time.**

### Act 1 tutorial

All are *reach* puzzles (`Goal = Reach`): touching a ball wins, as it did in the
prototype, where touching an objective scored. Buttons are the puzzle's
`AllowedMoveMask`; Execute is the planner's submit.

| # | File | Buttons | Grass | Lesson |
|---|---|---|---|---|
| 1 | `Act1/01_FirstSteps` | Forward | yes | A ball straight ahead: queue forward twice, Execute. |
| 2 | `Act1/02_Turning` | + turn left / right | yes | The ball is close but not in a straight line. |
| 3 | `Act1/03_Obstacle` | same | yes | A block in the way; hitting it destroys the ship (Reset and retry). Ball a little farther. |
| 4 | `Act1/04_Gathering` | same | yes | Three balls and three blocks; long plans cost more, so it takes several turns. |
| 5 | `Act1/05_UpHigh` | + up / down | yes | The ball is above the board. |
| 6 | `Act1/06_TheChaser` | all (adds wait) | no | A chaser. It steps toward the ship every tick, and turning is a tick spent standing still, so turning next to it gets you caught. Turn away from it first. |

Each has a verified solution in `GatherAndHunt.PuzzleRules.TutorialSolutions`,
which also checks that the tempting wrong play fails in 3 and 6.

Next: several one-ship puzzles of rising complexity (Act 1, from #7).

### Act 2 so far

`Act2/01_First` and `Act2/02_DuoShips` are the prototype's two-ship boards,
converted to reach puzzles with one ship moving per turn and no turn limit. Their
solutions haven't been checked yet.

## Structure

- Act 1: teach local movement grammar and consequence chains.
- Act 2: teach planning across multiple turtles with mild pressure.
- Act 3: stress parallel execution, tempo, and cross-board coordination.

Each act should end with a "mastery gate" puzzle that proves the player really understands the act's core mechanic rather than just stumbling through.

## Act 1: Flight School

**Theme:** one turtle at a time, low piece count, short horizons.

**Player fantasy:** "I understand how these turtles move."

**Mechanical goals:**

- Learn `Forward`, `YawLeft`, `YawRight`.
- Learn that pitch overrides heading for forward movement.
- Learn delivery off the board edge.
- Learn pickups versus scoring: objectives only score on delivery.
- Learn that collision order matters.

**Board shape:**

- Small boards.
- Very few hunters.
- Mostly hand-authored corridors, ramps, and chambers.
- Usually 1 active turtle, occasionally 2 where the second is inert or optional.

**Puzzle types:**

- Reach the exit in exactly N moves.
- Claim one objective, then escape.
- Use pitch to go over or under a blocker.
- Observe a hunter patrol and route around it.
- Learn to read a deterministic hazard loop or patrol.
- Deliberately sacrifice one turtle to let another survive.

**Escalation plan:**

1. Pure movement, no enemies.
2. Movement plus one static obstacle concept at a time.
3. First objective pickup and delivery.
4. First hunter avoidance puzzle.
5. First collision-order puzzle.

**Act 1 boss puzzle:**

A compact level with one objective, one hunter, and one forced pitch change where the shortest route fails because the turtle gets destroyed before it can deliver.

## Act 2: Split Attention

**Theme:** two to three turtles matter, but not yet at full chaos.

**Player fantasy:** "I can manage a team, not just a single route."

**Mechanical goals:**

- Plan around move budget across multiple turtles.
- Use one turtle as bait while another scores.
- Coordinate staggered arrivals to avoid same-cell pileups.
- Understand that an uncontested-looking route can fail once another turtle acts in the same tick.
- Introduce optional par scores and "better solution" incentives.

**Board shape:**

- Medium boards.
- 2 to 3 meaningful turtles.
- More open space, so path choice matters.
- Hunters positioned so distraction and routing matter more than raw avoidance.
- Deterministic circular and linear hazards can be mixed in to create timing gates without introducing randomness.

**Puzzle types:**

- Two-objective split: one turtle cannot score everything alone in time.
- Bait-and-run: one turtle pulls a hunter while another delivers.
- Traffic puzzle: two turtles must cross the same region without colliding.
- Budget puzzle: greedy routes exceed par, efficient coordination beats it.
- Soft fork puzzle: player can solve left-first or right-first, but only one ordering makes par.

**Escalation plan:**

1. Two turtles, independent goals.
2. Two turtles with shared space.
3. Two turtles with a bait role and a scorer role.
4. Three turtles where one exists mainly to alter board timing.
5. Par-score puzzles that reward elegant sequencing.

**Act 2 boss puzzle:**

A level where two turtles must enter the same central space on different ticks while a third turtle briefly lures a hunter out of position. The solution is not hard because of pathfinding; it is hard because of turn timing.

## Act 3: Parallel Machines

**Theme:** simultaneous movement is the puzzle.

**Player fantasy:** "I can think in full-turn choreography."

**Mechanical goals:**

- Make parallel execution the central challenge, not a side effect.
- Force the player to reason about multi-tick collision outcomes.
- Require planning around delivery timing, hunter movement, and objective pickup order at once.
- Create levels where the correct answer is a synchronized pattern, not a sequence of local fixes.
- Make failures legible: when the player loses, it should be because the parallel plan was wrong, not because the board was unreadable.

**Board shape:**

- Larger boards or denser medium boards.
- 3 to 4 active turtles.
- Shared lanes, contested chokepoints, mirrored spaces, and re-entry loops.
- Hunters used as moving timing constraints.
- Deterministic circular hazards and linear patrol hazards can act as "clockwork" blockers that define exact safe windows.

**Puzzle types:**

- Simultaneous crossing puzzle: two turtles must pass through adjacent lanes in lockstep or they collide.
- Delivery race puzzle: one turtle can score only if another vacates or occupies a cell on the correct tick.
- Hunter redirection puzzle: one turtle must intentionally become the nearest target at the right moment.
- Multi-claim puzzle: claim several objectives in one turn window, but only some deliveries are safe.
- Sacrifice calculus puzzle: losing a turtle is allowed, but only if it preserves the scoring line.

**Escalation plan:**

1. Teach synchronized arrivals.
2. Add hunters as timing pressure.
3. Add collision traps where "almost simultaneous" is wrong.
4. Add branching puzzles with multiple turtles whose roles swap mid-level.
5. End with full-board choreography where three or four turtles matter every turn.

**Act 3 boss puzzle:**

A three-stage choreography puzzle:

- Stage 1: one turtle pulls a hunter off-axis.
- Stage 2: two turtles cross a central chamber on offset ticks to avoid collision.
- Stage 3: a fourth turtle delivers last, but only if the earlier deliveries changed hunter priorities and board occupancy in the correct order.

The intended solution should feel like executing a machine: once understood, every move has a purpose and the whole board resolves cleanly.

## Campaign Pacing

- Keep Act 1 short and readable. It should create confidence.
- Make Act 2 the broadest act. This is where the player learns the full language of the game.
- Make Act 3 shorter but denser. The puzzle count can be lower because solve time will rise sharply.

A good first-pass level count is:

- Act 1: 10 to 12 puzzles.
- Act 2: 12 to 15 puzzles.
- Act 3: 8 to 10 puzzles.

## Authoring Rules

- Every puzzle should have a single teaching purpose, even if it also exercises older skills.
- Avoid "gotcha" hidden rules. If a puzzle depends on a collision edge case, stage a softer version earlier.
- Use par scores to separate "solved" from "mastered."
- For Act 3 especially, give the player clear visual reads on shared lanes and timing-critical cells.
- Reuse board motifs between acts so the player notices the shift from local routing to parallel choreography.

## Testing Implications

If single-player is arranged this way, the automated test strategy should eventually mirror it:

- Act 1 tests: movement legality, pitch transitions, delivery, pickup, simple hunter pursuit.
- Act 2 tests: budgeting, bait behavior, multi-turtle route interference, deterministic turn outcomes.
- Act 3 tests: exhaustive collision cases, delivery edge cases, multi-party timing, replay determinism, and serialized puzzle-state round trips.

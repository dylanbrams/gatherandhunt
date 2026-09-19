# Gather and Hunt — Build Specification

A multiplayer turn-based programming game in Unreal Engine 5. Players queue moves for a small fleet of "turtles" on a 3D grid, then both plans execute simultaneously. Goal: collect objectives and deliver them by escaping the grid before getting destroyed.

This document is the authoritative spec for the underlying systems. Read it end-to-end before writing code. Do not deviate from the architectural principles in §1 without flagging it.

---

## 1. Architectural Principles

These are non-negotiable. Every other decision in this document follows from them.

1. **Pure simulation core, zero UE dependencies.** The game logic lives in plain C++ classes — no `UObject`, no `AActor`, no `UWorld`, no `FName`, no Blueprint-callable annotations on simulation types. The core links against nothing UE-specific. This makes the simulation testable in isolation, deterministic, and trivially server-authoritative.
    
2. **Determinism is sacred.** Same `(InitialBoardState, Player1Plan, Player2Plan)` input → byte-identical event log out, every time, every machine. No `rand()`, no floating point in the simulation, no iteration over unordered containers, no pointer-based comparisons. If you need randomness (board generation), it is seeded and uses a fixed PCG implementation we ship in the core.
    
3. **Discrete everything.** Positions are `int32` triples. Orientations are enums. Time advances in integer ticks. There is no physics. The renderer interpolates for visual smoothness; the simulation never does.
    
4. **State + events, not state + state.** The resolver consumes a board state and two plans and produces an ordered event log. Clients reconstruct intermediate states by applying the log to their copy of the initial state. Replays are just `(initial_state, plan_history)`.
    
5. **Actors are dumb.** UE actors subscribe to simulation events and animate. They never make gameplay decisions, never own gameplay state, never call into the simulation except through read-only views.
    
6. **Server is the only authority.** Clients run the same simulation locally for previews and animation, but their results are advisory. The server's event log is truth. The server also needs to be able to do 'vetting' of move lists as well as the client does so multiplayer stays deterministically fair.
    

---

## 2. Project Layout

```
Source/
  TurtleSim/                     # Static lib, pure C++, NO UE includes
    Public/
      BoardState.h
      Move.h
      Orientation.h
      TickResult.h
      Resolver.h
      HunterAI.h
      BoardGenerator.h
      CostModel.h
      Pathfinding.h
      RandomPCG.h
    Private/
      *.cpp
    Tests/
      ResolverTests.cpp
      CollisionTests.cpp
      BoardGenTests.cpp
      DeterminismFuzz.cpp
  TurtleGame/                    # UE module, depends on TurtleSim
    Public/
      Net/                       # RPCs, replication structs
      Match/                     # GameMode, GameState, PlayerState
      Actors/                    # ATurtleActor, AObstacleActor, ...
      UI/                        # Planner widgets
      Playback/                  # Tick log → animation
    Private/
      *.cpp
```

`TurtleSim` must compile and its tests must run **without launching UE at all**. Use a tiny CMake or standalone Visual Studio project alongside the UE build. This is the single biggest investment in development speed you can make on this project.

---

## 3. Coordinate System & Constants

- World axes: `+X` = "east", `+Y` = "north", `+Z` = "up". Right-handed.
- Grid cells are unit cubes. Cell `(x, y, z)` occupies the volume from `(x, y, z)` to `(x+1, y+1, z+1)` in world space; the turtle's logical position is the integer triple.
- Board size is `(SizeX, SizeY, SizeZ)`. All three are odd. Default: `11 × 11 × 11`.
- The "player axis" is the axis the two players sit across. Default: the X axis. Player 1 spawns near `x = 0`, Player 2 near `x = SizeX - 1`.
- Symmetry plane: the plane `x = (SizeX - 1) / 2`. Board generator must produce boards where reflection across this plane maps player 1's setup onto player 2's exactly.

```cpp
// Constants.h
inline constexpr int32 DefaultBoardSize = 11;
inline constexpr int32 DefaultMoveBudget = 20;
inline constexpr int32 MaxTurtlesPerPlayer = 5;
inline constexpr int32 MinTurtlesPerPlayer = 3;
```

---

## 4. Orientation System

**Two-axis rotation only.** No roll. The model is essentially FPS-style: turtles have a horizontal heading (one of 4) and a pitch (one of 3). The forward movement direction is derived from these.

```cpp
// Orientation.h
enum class EHeading : uint8 {
    North = 0,  // +Y
    East  = 1,  // +X
    South = 2,  // -Y
    West  = 3   // -X
};

enum class EPitch : uint8 {
    Down  = 0,  // forward = -Z (heading still tracked, used when leveling out)
    Level = 1,  // forward = heading direction
    Up    = 2   // forward = +Z
};

struct FOrientation {
    EHeading Heading = EHeading::North;
    EPitch   Pitch   = EPitch::Level;

    FIntVector ForwardVector() const;   // returns one of 6 unit vectors
    bool operator==(const FOrientation&) const;
};
```

**Why this representation:** A single 6-direction enum loses the player's horizontal facing when they pitch up or down, which makes "look down, then back up to where I was" impossible to express cleanly. The (Heading, Pitch) pair preserves heading across pitch changes. 12 states total.

**Movement direction:**

- `Pitch::Level` → forward is the unit vector for `Heading`
- `Pitch::Up` → forward is `(0, 0, +1)`
- `Pitch::Down` → forward is `(0, 0, -1)`

**Turn semantics:**

- `YawLeft` / `YawRight`: rotates `Heading` by ±1 mod 4. Always valid, always cheap. Heading changes even when pitched (you can rotate while looking up — it just changes which direction you'll face when you level out).
- `PitchUp`: `Down → Level → Up`. Already at `Up` is invalid (rejected at validation, doesn't consume budget).
- `PitchDown`: `Up → Level → Down`. Already at `Down` is invalid.

---

## 5. Data Structures

All POD-style. Trivially serializable. Use `int32` and fixed-width types only — no `int`, no `size_t` in serialized structs.

```cpp
// BoardState.h
using FTurtleId   = int32;
using FObstacleId = int32;
using FObjectiveId = int32;
using FPlayerId   = int32;  // 0 or 1

struct FTurtle {
    FTurtleId    Id;
    FPlayerId    Owner;
    FIntVector   Position;
    FOrientation Orientation;
    bool         bAlive          = true;
    bool         bDelivered      = false;  // exited the grid carrying points
    int32        CarriedPoints   = 0;
};

enum class EObstacleKind : uint8 {
    Static,    // never moves
    Wandering, // moves on a deterministic pattern (TBD; default = no movement v1)
    Hunter     // moves toward nearest living turtle, 1 cell per tick
};

struct FObstacle {
    FObstacleId  Id;
    EObstacleKind Kind;
    FIntVector   Position;
    bool         bAlive = true;
};

struct FObjective {
    FObjectiveId Id;
    FIntVector   Position;
    int32        PointValue;     // 1, 3, 5, or 10
    bool         bClaimed = false;
};

struct FBoardState {
    FIntVector             Size;
    int32                  TurnNumber = 0;
    TArray<FTurtle>        Turtles;
    TArray<FObstacle>      Obstacles;
    TArray<FObjective>     Objectives;
    int32                  ScorePlayer0 = 0;
    int32                  ScorePlayer1 = 0;
    uint32                 Seed = 0;     // for determinism audits

    // Helpers
    bool   IsInBounds(FIntVector P) const;
    const FTurtle* FindTurtle(FTurtleId) const;
    FTurtle* FindTurtleMut(FTurtleId);
    // ... similar for obstacle/objective
};
```

Serialization: write a `Serialize(FArchive&)` for each struct following standard UE conventions, BUT the archive must be deterministic (no float quantization, no asset references). For the simulation core's own tests, ship a tiny binary writer/reader that doesn't depend on UE.

---

## 6. Move Primitives & Plans

```cpp
// Move.h
enum class EMoveKind : uint8 {
    Forward    = 0,
    YawLeft    = 1,
    YawRight   = 2,
    PitchUp    = 3,
    PitchDown  = 4,
    Wait       = 5
};

struct FMove {
    EMoveKind Kind;
};

struct FTurtlePlan {
    FTurtleId         TurtleId;
    TArray<FMove>     Moves;        // in order; index = tick within turn
};

struct FTurnPlan {
    FPlayerId             PlayerId;
    TArray<FTurtlePlan>   PerTurtle;  // one entry per living turtle owned by this player
};
```

Plans are the entire input to the resolver alongside the board state.

---

## 7. Cost Model

Per-turtle cost lookup. The cost is per-move-position-in-that-turtle's-queue. Total turn budget is shared across all turtles for that player.

```cpp
// CostModel.h
namespace CostModel {
    // Cost of the Nth move (1-indexed) in a single turtle's queue.
    inline constexpr int32 MoveCost(int32 OneIndexedMoveNumber) {
        if (OneIndexedMoveNumber <= 6)  return 1;
        if (OneIndexedMoveNumber <= 10) return 2;
        return 3;
    }

    // Total cost of a single turtle's plan.
    int32 PlanCost(const FTurtlePlan& Plan);

    // Total cost across all turtles in a turn.
    int32 TurnCost(const FTurnPlan& Plan);
}
```

**Reference table:**

|Move #|Cost|Cumulative|
|---|---|---|
|1|1|1|
|2|1|2|
|3|1|3|
|4|1|4|
|5|1|5|
|6|1|6|
|7|2|8|
|8|2|10|
|9|2|12|
|10|2|14|
|11|3|17|
|12|3|20|

A single turtle sprinting alone exhausts the full 20-budget at move 12. Spread across turtles, you can do many more total moves.

`Wait` and turn moves count toward the queue position the same as `Forward`. That's the rule: position in queue, not type, determines cost. (This is a tunable knob — see §17.)

---

## 8. Plan Validation

```cpp
// Resolver.h
enum class EPlanValidationError : uint8 {
    None,
    BudgetExceeded,
    TurtleNotOwned,
    TurtleNotAlive,
    DuplicateTurtleEntry,
    InvalidPitchTransition,   // PitchUp at Up, etc.
    EmptyMove                 // a kind value out of range
};

struct FPlanValidation {
    EPlanValidationError Error = EPlanValidationError::None;
    int32 OffendingTurtle = -1;
    int32 OffendingMoveIndex = -1;
};

FPlanValidation ValidateTurnPlan(const FBoardState& State, const FTurnPlan& Plan);
```

Validation is **side-effect free** and runs identically on client (for live UI feedback) and server (authoritative). Validation does NOT check whether moves run a turtle into walls, obstacles, or other turtles — that's resolved at simulation time. Validation only catches what's locally checkable per-turtle: budget, ownership, life, pitch transitions, well-formedness.

**Why no wall-check at validation:** the player can't always predict if they'll be alive by tick N (an opponent might destroy them), or if a wandering obstacle will be in the way. Letting plans be "optimistic" is correct; the simulation handles the consequences.

---

## 9. Tick Resolver — The Heart

The single most important function in the codebase. Pure, total, deterministic.

```cpp
// Resolver.h
enum class EEventType : uint8 {
    TurtleMoved,
    TurtleTurned,
    TurtleWaited,
    TurtleBlocked,           // attempted move into wall (invalid forward)
    TurtleDestroyed,
    TurtleDelivered,         // exited grid carrying points
    TurtleClaimedObjective,
    ObstacleMoved,
    ObstacleDestroyed,
    HunterRetargeted         // optional, for visual flavor
};

struct FTickEvent {
    EEventType  Type;
    int32       PrimaryId;       // turtle or obstacle id
    int32       SecondaryId;     // -1 if N/A; else target/objective/destroyer
    FIntVector  FromPosition;    // valid for movement
    FIntVector  ToPosition;
    FOrientation FromOrientation;
    FOrientation ToOrientation;
    int32       PointsDelta;     // for claims / deliveries
    // Add more as needed; keep struct flat and POD.
};

struct FTickResult {
    int32                TickIndex;       // 0-based within turn
    TArray<FTickEvent>   Events;          // ordered, deterministic
};

struct FTurnResult {
    TArray<FTickResult>  Ticks;
    FBoardState          FinalState;      // convenience; clients can recompute
};

FTurnResult ResolveTurn(const FBoardState& InitialState,
                        const FTurnPlan& Plan0,
                        const FTurnPlan& Plan1);
```

### 9.1 Resolution algorithm, per tick

A "tick" is one step within a turn. Number of ticks = max plan length across both players. Turtles whose plan is shorter act as if they had `Wait` moves appended (or simply don't act after their plan ends — see §17).

For each tick `t`:

1. **Collect intents.** For every living, undelivered turtle:
    
    - If `t < plan.Moves.Num()`, the move is `plan.Moves[t]`. Otherwise no intent.
    - Compute the _intended next state_ (position + orientation) assuming nothing else exists. Turn moves only change orientation; `Forward` changes position; `Wait` does nothing.
    - For `Forward`: compute target cell. If target is outside the grid, this is a **delivery intent** (handled in step 5). If inside, it's a normal move intent.
2. **Collect obstacle intents.** For each living `Hunter`, run §10 to compute its intended next cell. For `Static`, no intent. (`Wandering` deferred to v1+.)
    
3. **Compute pitch/yaw resolution first.** Turn moves don't move anything spatially; they just change orientation. Apply them immediately and emit `TurtleTurned` events. They can't collide with anything.
    
4. **Resolve spatial intents simultaneously.** This is §10. Output:
    
    - Final positions for entities that survived
    - List of destroyed entities
    - Edge-of-grid exits
5. **Resolve deliveries.** Any turtle whose intended move went off the grid AND survived collision handling (it didn't get hit by something while leaving): mark `bDelivered = true`, add `CarriedPoints` to owner's score, emit `TurtleDelivered`, remove from active set. Carried points come from previously claimed objectives this turn (see step 6 of prior ticks).
    
6. **Resolve pickups.** Any turtle that ended this tick on an unclaimed objective cell claims it. Add `PointValue` to that turtle's `CarriedPoints`. Mark objective `bClaimed = true`. Emit `TurtleClaimedObjective`. **Important:** points only score when the turtle delivers. A turtle destroyed while carrying points loses them — those points are forfeit.
    
7. **Increment tick. Repeat until no more intents from any party.**
    

After all ticks, finalize the board state and return.

### 9.2 Why this exact order

- Turns before moves: a player rotating in place doesn't conflict with anyone, simplest to dispatch first.
- All spatial moves at once: this is the "everything moves simultaneously" rule. No advantage to plan order.
- Deliveries after collisions: a turtle stepping off the grid is still vulnerable to being rammed in the same tick from behind by a hunter — it must survive collision resolution to deliver.
- Pickups last: prevents a destroyed turtle from claiming an objective it died on.

---

## 10. Collision Resolution

The trickiest part. Get this right with exhaustive tests before anything else.

### 10.1 Inputs

For each living entity (turtles + moving obstacles), an "intent": `(Id, FromCell, ToCell, FacingFrom, IntentKind)`. `IntentKind` distinguishes turtle/hunter/etc. for tie-breaking.

Static obstacles have no intent but occupy their cell.

### 10.2 Algorithm

Build two maps:

- `OccupantsByDestination: FIntVector → TArray<Intent>` (everything trying to enter that cell, including static obstacles whose `From == To` is their cell)
- `OriginsByCell: FIntVector → TArray<Intent>` (where each entity is leaving from)

Then resolve:

**Step A — Position swap (head-on through an edge):** For each pair `(I1, I2)` where `I1.From == I2.To` and `I1.To == I2.From`: this is a head-on collision through the shared face. Both entities are destroyed. (This catches the case where two turtles "trade places" — physically impossible, must collide.)

**Step B — Same-destination collisions:** For each cell `C` with multiple intents arriving:

- If any pair has _opposite_ facings (heads pointing at each other) → head-on at `C`. All entities arriving at `C` from opposite-facing pairs are destroyed.
- For remaining entities arriving at `C` with non-opposing facings → all destroyed (sideswipe pile-up). This is the simplest deterministic rule. Tunable.

**Step C — Static obstacle collisions:** For each cell `C` containing a static obstacle: any non-static entity intending to enter `C` is destroyed. The static obstacle is unharmed. (Tunable: ramming static obstacles could destroy them too, but default is no.)

**Step D — Hit-from-side / hit-from-rear into a stationary turtle:** A turtle that didn't move (no `Forward` intent — was waiting or turning) is "stationary at its cell." Any other turtle whose intent enters that cell:

- If facings are opposite → head-on, both destroyed.
- Otherwise → the moving turtle destroys the stationary one (rear/side hit). The mover survives and ends at that cell.

**Step E — Apply survivors:** Each surviving entity moves to its intent's `To` cell. Emit `TurtleMoved` / `ObstacleMoved` / `TurtleDestroyed` / `ObstacleDestroyed` events in a deterministic order: by `(EntityKind, EntityId)` ascending.

### 10.3 Edge cases to test exhaustively

Write a test for every one of these. They are the spec.

|#|Scenario|Expected outcome|
|---|---|---|
|1|Two turtles, head-on into same cell|Both destroyed|
|2|Two turtles, swap cells (each enters the other's origin)|Both destroyed (position-swap)|
|3|Turtle A enters cell occupied by stationary turtle B; A facing into B's back|B destroyed, A occupies cell|
|4|A enters cell occupied by stationary B; A facing into B's side|B destroyed, A occupies cell|
|5|A enters cell occupied by stationary B; A facing into B's front (B is just facing toward A but not moving)|Both destroyed (head-on counts even if one is stationary)|
|6|Three turtles all enter the same cell from three sides|All three destroyed (pile-up)|
|7|Turtle moves into a static obstacle|Turtle destroyed, obstacle unharmed|
|8|Turtle moves into a hunter's cell while hunter is moving away|Turtle survives if hunter vacated the cell this tick, else collision at hunter's destination|
|9|Turtle attempts forward into the grid edge (delivery direction)|Marked as delivery intent, processed in step 5|
|10|Turtle delivers while another turtle rams it from behind same tick|Rammer destroys turtle before delivery resolves; points forfeit|
|11|Two turtles deliver off the same edge cell same tick|Both deliver successfully (no collision; they're leaving the play area)|
|12|Hunter and turtle swap cells|Both destroyed (hunter dies too — tunable)|
|13|Turtle waits/turns; another turtle moves into its cell|Stationary turtle destroyed|
|14|Two turtles both "Wait" on adjacent cells|Nothing happens|

### 10.4 Determinism

Every iteration over collected intents must be in `(EntityKind, EntityId)` order. Use sorted arrays, never `TMap` iteration without a key sort. The event log must be byte-identical across runs.

---

## 11. Hunter AI

```cpp
// HunterAI.h
struct FHunterMoveDecision {
    FObstacleId HunterId;
    FIntVector  IntendedCell;   // == current cell if no path
};

TArray<FHunterMoveDecision> ComputeHunterMoves(const FBoardState& State);
```

### 11.1 Algorithm

For each living `Hunter`:

1. Run BFS over grid cells, treating other obstacles (alive) and out-of-bounds as blocked. Start from hunter's cell.
2. Stop when any living, undelivered turtle is reached. Distance is path length (Manhattan-on-grid, not Euclidean).
3. If multiple turtles tie on shortest path, pick the one with the lowest `FTurtleId` (deterministic tie-break).
4. Walk the path back; first step from hunter's cell is the intended move.
5. If no path exists (turtle unreachable): hunter does not move this tick.

**Do not** use A*; the boards are small enough that BFS is fine and BFS guarantees shortest path with no heuristic-tiebreak nondeterminism.

**Do not** treat turtles as blocking during BFS — turtles are the targets. Treat only obstacles and bounds as blocking.

Hunter intents go into the same collision resolver as turtle intents (§10). A hunter can be destroyed by ramming a turtle if the rules call for it (case 12 in §10.3), or via head-on collision with another hunter.

---

## 12. Board Generator

```cpp
// BoardGenerator.h
struct FBoardGenParams {
    FIntVector Size = {11, 11, 11};
    int32      TurtlesPerPlayer = 4;
    int32      ObjectiveCount   = 12;     // total, distributed by mirror
    int32      StaticObstacles  = 8;      // total
    int32      Hunters          = 2;      // total
    // Point distribution; values must be one of {1, 3, 5, 10}
    TArray<int32> ObjectivePointPool;
    uint32     Seed = 0;
};

FBoardState GenerateBoard(const FBoardGenParams& Params);
```

### 12.1 Algorithm

1. Seed a PCG random generator (`RandomPCG.h` — ship our own; do NOT use `FMath::Rand`).
2. Compute the half-grid: cells where `x < (SizeX-1)/2` (the player 1 side, exclusive of midplane).
3. Place player 1's spawn turtles:
    - All on `x = 0` plane (or `x = 0..1` for variety)
    - Spread across Y/Z so they're not stacked
    - All facing East (toward midplane)
4. Place static obstacles randomly in the half-grid, avoiding spawn cells. Mirror each to the other side.
5. Place hunters similarly, mirrored.
6. Place objectives. Distribute the `ObjectivePointPool` values across cells in the half-grid. Mirror.
7. Place midplane content: optionally allow a small set of objectives or neutral-ish obstacles on the midplane itself (these are self-mirror-symmetric).
8. **Validate the board:**
    - Every player has at least one path from each spawn to at least three objectives (BFS, treating obstacles as blocked but ignoring hunters).
    - Total point value reachable from each spawn is ≥ some threshold.
    - No spawn is boxed in (at least 2 of 6 neighbors free).
    - If validation fails, increment a retry counter and try again with a perturbed seed. After N failures, return an error (caller should handle by re-rolling).

### 12.2 Mirror logic

```cpp
FIntVector Mirror(FIntVector P, int32 SizeX) {
    return FIntVector(SizeX - 1 - P.X, P.Y, P.Z);
}

EHeading MirrorHeading(EHeading H) {
    switch (H) {
        case EHeading::East:  return EHeading::West;
        case EHeading::West:  return EHeading::East;
        default:              return H;  // North/South unchanged
    }
}
```

---

## 13. Match State Machine

Lives on the server. Use a `UGameInstanceSubsystem` or the `AGameModeBase` — your call. The state machine is independent of UE replication; it just drives the flow.

```cpp
enum class EMatchPhase : uint8 {
    WaitingForPlayers,
    Planning,            // both players see board; clock running
    AwaitingPlans,       // one player submitted, waiting on other
    Resolving,           // server is computing the turn
    Animating,           // clients are playing back; server waits before next turn
    Ended
};
```

**Transitions:**

- `WaitingForPlayers → Planning`: both players connected and ready. Server generates board (or uses preconfigured one) and replicates initial state.
- `Planning → AwaitingPlans`: first player submits plan. Validate immediately. If invalid, reject and stay in `Planning` for that player.
- `Planning → Resolving`: timer expires. Auto-submit empty plans for whoever didn't submit.
- `AwaitingPlans → Resolving`: second player submits.
- `Resolving → Animating`: server runs `ResolveTurn`, builds tick log, broadcasts.
- `Animating → Planning`: both clients confirm playback complete (or timeout). Increment turn number. Check end conditions.
- `* → Ended`: end conditions met — see below.

**End conditions (any one triggers `Ended`):**

- All objectives claimed AND all carrying turtles delivered or destroyed.
- One player has zero living turtles.
- Turn limit reached (default 20 turns; tunable).
- Both players surrender / disconnect.

Winner is determined by score. Tiebreak: most turtles delivered, then most turtles surviving, then a draw.

---

## 14. Networking

Keep this layer thin. The simulation is the contract; networking just shuttles plans and event logs.

### 14.1 Replicated state

```cpp
// Replicated on the GameState. Updates only at phase transitions.
USTRUCT()
struct FReplicatedMatchState {
    UPROPERTY()
    TArray<uint8> SerializedBoardState;   // FBoardState binary blob

    UPROPERTY()
    int32 TurnNumber = 0;

    UPROPERTY()
    EMatchPhase Phase = EMatchPhase::WaitingForPlayers;

    UPROPERTY()
    int32 ScorePlayer0 = 0;

    UPROPERTY()
    int32 ScorePlayer1 = 0;
};
```

Clients reconstruct `FBoardState` from the blob. This is the only authoritative state replication — everything else flows through events.

### 14.2 RPCs

```cpp
// Client → Server
UFUNCTION(Server, Reliable)
void ServerSubmitTurnPlan(const TArray<uint8>& SerializedPlan);

// Server → Client
UFUNCTION(Client, Reliable)
void ClientReceiveTickLog(const TArray<uint8>& SerializedTurnResult);

// Server → Client (rare; for desync recovery)
UFUNCTION(Client, Reliable)
void ClientReceiveFullState(const TArray<uint8>& SerializedBoardState);
```

### 14.3 Hidden information

A player must not see the opponent's plan until both have submitted and resolution is complete. The server does not broadcast either plan — it broadcasts only the resulting tick log, which reveals what the opponent's turtles did via observed events. This is correct: the opponent's queued moves are private; their executed moves are public.

### 14.4 Reconnect

On reconnect, server sends `ClientReceiveFullState` with the current `FBoardState` and the current phase. If reconnecting mid-`Resolving`/`Animating`, server can re-send the most recent tick log so the client catches up.

---

## 15. Visualization & Playback

UE actor classes, all dumb. They subscribe to a playback controller; they don't query game state directly.

```cpp
class ATurtleActor : public AActor {
    // Properties for visual state only:
    //   target position, target orientation, alive flag
    // No FTurtle reference; no FBoardState reference.
    void ApplyEvent(const FTickEvent& Event);
    void Tick(float DeltaTime) override;  // interpolates toward target
};
```

`ATurtlePlaybackController` (a `UActorComponent` on the GameState or a dedicated actor):

```cpp
class UTurtlePlaybackController : public UActorComponent {
    void StartPlayback(const FTurnResult& Result);
    void Tick(float DeltaTime) override;

    // Configurable
    float SecondsPerTick = 0.6f;
    bool  bPaused = false;

    // Stepping (for debug/replay)
    void Pause();
    void Resume();
    void StepOne();
};
```

The controller walks the tick log, dispatching events to the appropriate actors at the right wall-clock pace. Actors interpolate.

**No actor ever modifies `FBoardState`.** The client maintains its own `FBoardState` mirror, applies the tick log to it as playback completes, and uses that as the source of truth for the next planning phase's UI.

---

## 16. Move Planner UI

The most player-facing system. Built with UMG.

### 16.1 Components

- **Turtle list panel:** lists each of the player's living turtles. Click to select.
- **Move queue editor:** for selected turtle, shows the queued moves as a horizontal strip. Drag to reorder, click to delete. Five buttons (or keys) to add: Forward, YawLeft, YawRight, PitchUp, PitchDown, Wait.
- **Budget meter:** shows current cost / 20. Updates live.
- **Per-turtle cost breakdown:** shows each turtle's plan cost and any sprint-tier indicator (move 7+, 11+).
- **3D ghost preview:** for the selected turtle, draws a translucent path through the grid showing where each queued move will take it (assuming no opposition). Updates live as moves are added/removed.
- **Validation warnings:** if the plan has a `PitchUp` from `Up` etc., highlight the offending move.
- **Submit button:** disabled while invalid; enabled otherwise. Sends plan via RPC.

### 16.2 Preview simulator

This is **the same `ResolveTurn` function from the simulation core**, called locally. The client builds a hypothetical opponent plan (default: empty/all-Wait), runs the resolver against the current board state, and shows the result as a "what if I'm uncontested" preview. The player can toggle this on/off.

The fact that it's the same code as the server means previews are exact under the assumption of no opponent action — no drift, no separate maintenance.

---

## 17. Tunable Knobs (consolidated)

Centralize these so they're easy to find and adjust. Put them in a `Config/GameTuning.ini` and load via a `UGameTuningSettings` if you want them runtime-tunable, or just `inline constexpr` in `Constants.h` if not.

|Knob|Default|Notes|
|---|---|---|
|Board size|11×11×11|Always odd in all axes|
|Turtles per player|4|Range 3–5|
|Move budget per turn|20||
|Cost tier 1|moves 1–6, cost 1||
|Cost tier 2|moves 7–10, cost 2||
|Cost tier 3|moves 11+, cost 3||
|Turn count for `Wait`|counts toward queue position|Tunable: could make `Wait` free in cost|
|Hunter behavior on contact with turtle|both destroyed|Tunable to "hunter survives"|
|Multi-turtle pile-up|all destroyed|Tunable to "first arrival wins by Id"|
|Plan-too-short turtles|act as `Wait` for remaining ticks|Tunable to "stop participating"|
|Static obstacle on ram|obstacle unharmed|Tunable to "destructible"|
|Per-turn timer|90 seconds||
|Match turn limit|20 turns||
|Objective point pool|mostly 1s and 3s, few 5s, rare 10|Tune for game length|
|Hunters per match|2|Increase for more chaos|

---

## 18. Test Harness

Build this **before** wiring anything to UE. It's the thing that makes the whole project tractable.

### 18.1 Unit tests (run on every build)

- Every collision case in §10.3 — one test each
- Cost model: every move number from 1 to 30 returns expected cost
- Cost model: `PlanCost` and `TurnCost` correct on assorted plans
- Validation: every error type triggers correctly
- Orientation: `ForwardVector` correct for all 12 (Heading, Pitch) combinations
- Orientation: yaw cycles back to original after 4 turns; pitch saturates correctly
- Hunter AI: shortest path on simple boards, deterministic tie-breaking, no-path case
- Board generator: every produced board passes its own validation; mirror symmetry holds exactly for 100 random seeds

### 18.2 Golden replay tests

Hand-craft 5–10 scenarios (initial state + both plans) and check `ResolveTurn` produces an exact expected event log. These catch regressions when you tweak collision rules.

### 18.3 Determinism fuzz

```cpp
// Pseudocode
for i in 0..1000:
    seed = i
    state = GenerateBoard({.Seed = seed})
    plan0 = RandomPlan(state, player=0, seed=seed*2)
    plan1 = RandomPlan(state, player=1, seed=seed*2+1)

    result_a = ResolveTurn(state, plan0, plan1)
    result_b = ResolveTurn(state, plan0, plan1)
    assert SerializedEqual(result_a, result_b)
```

Run this in CI on every commit to the simulation core. It's cheap (the resolver is fast) and catches an entire class of nasty bugs.

### 18.4 Headless soak

Optional but high-value: a script that plays N full matches with random plans both sides, asserts that no match crashes, no match exceeds turn limit without resolving, scores are non-negative, and event logs are well-formed. Run for thousands of matches.

---

## 19. Replay System

Almost free given the architecture. Save:

```cpp
struct FMatchReplay {
    FBoardState                 InitialState;
    TArray<FTurnPlan>           Player0Plans;   // one per turn
    TArray<FTurnPlan>           Player1Plans;
    int32                       FinalScore0;
    int32                       FinalScore1;
    FString                     Player0Name;
    FString                     Player1Name;
    FDateTime                   PlayedAt;
    FString                     ClientVersion;
};
```

To play back: feed the initial state into the resolver, apply `Player0Plans[0]` and `Player1Plans[0]`, render the result; advance state; repeat. Same engine as live play. Build the replay viewer as a thin wrapper around the playback controller.

This data is tiny — a full match is on the order of a few KB serialized. Save them all, locally and (later) server-side. Worth wiring on day one.

---

## 20. Implementation Order

Do not deviate from this order. Each step builds on the last and is independently testable.

### Phase 1 — Simulation core (no UE)

1. Project skeleton: `TurtleSim` static lib + standalone test runner.
2. `Orientation.h`, `BoardState.h`, basic types. With unit tests.
3. `CostModel.h` + tests.
4. `RandomPCG.h` (steal an MIT-licensed PCG impl; ship it in-tree).
5. `ValidateTurnPlan` + tests.
6. `Resolver.h` skeleton: just dispatches turns and waits, no movement. Tests for trivial cases.
7. Movement intent collection + simple movement (no collisions). Tests.
8. **Collision resolution** — the §10 algorithm, all 14 test cases must pass.
9. Delivery (off-grid forward) + carried points.
10. Pickups.
11. Hunter AI + integration into resolver.
12. Static obstacle handling (it's almost trivial after collisions).
13. `BoardGenerator` + symmetry tests.
14. Determinism fuzz harness — runs 1000 random matches, assertion clean.

**Gate:** before moving to Phase 2, the simulation must pass all unit tests, all golden replays, and the fuzz harness on a clean run.

### Phase 2 — UE integration

15. `TurtleGame` module. Set up dependency on `TurtleSim`.
16. `AGameMode` + match state machine. No UI yet; drive it via console commands.
17. Replicated `FReplicatedMatchState`. Confirm clients receive board state.
18. RPCs: `ServerSubmitTurnPlan`, `ClientReceiveTickLog`. Send hardcoded plans from each client; verify resolution and broadcast.
19. Headless multiplayer test: two dedicated server + bot client setup that submits random plans and runs to completion.

### Phase 3 — Visualization

20. `ATurtleActor`, `AObstacleActor`, `AObjectiveActor`. Static placement matching board state.
21. `UTurtlePlaybackController`. Play tick log; actors interpolate.
22. Camera, lighting, basic VFX hooks (destroyed, delivered, claimed).

### Phase 4 — Planner UI

23. UMG widgets per §16. Plan editor, budget meter.
24. Local preview simulator (calls `ResolveTurn` with empty opponent plan).
25. Submit flow + validation feedback.

### Phase 5 — Polish

26. Replay save/load.
27. Reconnect handling.
28. Spectator mode (just a connected client that doesn't have a player slot; receives all tick logs).
29. End-of-match screen, score breakdown, replay link.

---

## 21. Open Design Knobs (decide before Phase 1)

Lock these down or explicitly defer them. Don't let them float.

1. **Deterministic moving hazards** — include for puzzle authoring. They must use fixed, inspectable patterns only: circular loops or linear back-and-forth patrols. No random wandering hazards.
2. **Wait cost** — is `Wait` free, or does it count toward the queue position like every other move? Spec assumes the latter.
3. **Plan-too-short** — turtle does nothing for the rest of the turn vs. acts as if `Wait`? Spec assumes Wait. Affects how vulnerable a "done" turtle is.
4. **Hunter destruction by turtle ram** — does a hunter die when a turtle hits it from behind, the way a turtle would? Spec assumes yes (symmetric with turtle-on-turtle). Tunable.
5. **Pickup contention** — if two turtles from different players land on the same objective same tick (after collision resolution leaves both alive), who claims it? Spec is silent. Suggested: lower `FTurtleId` wins; or split if both survive. Make a call before §9 step 6.
6. **Forfeit on disconnect** — partial-turn disconnect: does the disconnected player get their submitted plan executed if they had submitted, or does the match abort? Suggested: execute submitted plans; abort only if both disconnect.

---

## 22. Single-Player Puzzle Mode (Planned, post-v1)

The original UE 5.4 prototype was built around hand-authored single-player puzzles, and that mode is a first-class planned feature — not just a tutorial. It ships **after** multiplayer is validated, but the simulation core was designed with this in mind: the same `ResolveTurn` runs both modes.

**Feel.** Tutorial-to-hard difficulty curve. Early levels teach one mechanic at a time (forward, yaw, pitch, picking up an objective, avoiding a hunter, delivering off the edge). Later levels stack constraints: tight move budgets, precise timing against hunters, multi-turtle coordination, rooms that force you to use pitch. End-game levels are real puzzles — there is one (or a small set of) intended optimal solutions, scored against par.

**How it reuses the core:**

- Levels are hand-authored `BoardState` snapshots, loaded from disk. The board generator (§12) is bypassed.
- The "opponent" in the resolver is a scripted `FTurnPlan` per turn (often empty, sometimes pre-recorded for adversarial puzzle setups). Hunters and obstacles still use the same code paths.
- Scoring is against par (move count, turns used, points collected) rather than versus an opponent. The win condition is per-level: e.g. "deliver N points within K turns," "destroy hunter X," "claim all objectives without losing a turtle."
- No replication. Single-player runs the resolver locally and animates with the same `UTurtlePlaybackController` (§15).

**Authoring.** Levels are stored as the same serialized `FBoardState` blob §14.1 uses on the wire, plus a small `FPuzzleMeta` (par, win condition, intro text, optional scripted opponent plans). The legacy UE 5.4 level editor is salvageable as the puzzle authoring tool — it doesn't need to be re-implemented from scratch, just rewired to emit the new format.

The practical runtime shape should therefore be: `ULevelSave` authoring data -> local puzzle session -> shared deterministic resolver. Scripted opponent turns are looked up by turn number and fed into the same `ResolveTurn` call as the player's plan; they are not a separate special-case simulation.

**Level editor requirements.** This needs to be treated as a real system, not a vague content-side convenience. The intended implementation is an Unreal editor-facing authoring backend that drives the existing editing map and Blueprint tooling:

- **Single source of truth:** authoring data lives in a `ULevelSave`/piece-save representation that can be serialized directly to disk and later converted into the runtime `BoardState`/puzzle wrapper.
- **Puzzle metadata:** the same authored object must also carry puzzle-level metadata such as intro text, target score, target turn count, act/order placement, grid cell size, and any scripted opponent plans.
- **Core editor actions:** create a puzzle, add/update/remove pieces, normalize piece ids, validate bounds and authored parameters, preview deterministic hazard motion, save to disk, and load an existing puzzle for revision.
- **Deterministic validation:** the editor must reject malformed puzzles early — duplicate piece ids, out-of-bounds placements, impossible target objective counts, and invalid hazard parameters should fail validation before a puzzle is considered shippable.
- **Hazard preview:** circular and linear patrol hazards must expose a turn-by-turn preview path in-editor so authors can see the exact repeated loop the player is expected to reason about.
- **Blueprint-friendly surface:** the backend should be callable from Blueprint/editor utility tools so the old content pipeline can be reused, but the save format logic itself belongs in C++ for determinism and testability.
- **Round-trip safety:** save/load has to preserve every authored field required for puzzle behavior, including hazard phase offsets and patrol dimensions. This path needs automated tests, because silent data loss here will make puzzle debugging miserable.

**Deterministic hazard patterns.** Puzzle levels may also include hand-authored moving hazards that are not hunter AI driven. These hazards must be fully deterministic and readable from the board state:

- **Circular hazard:** follows a fixed loop of cells in a repeating cycle. The loop ordering is authored up front and never randomized at runtime.
- **Linear patrol hazard:** moves back and forth along a fixed line segment, reversing direction only at its authored endpoints.
- **No random movement:** hazards never choose a move stochastically. Given the same initial board state and turn number, the same hazard must always produce the same intent.
- **Authored parameters:** hazard motion is defined by a start position, a facing/orientation, a primary step count, an optional secondary step count (for rectangular circular loops), a fixed step distance, and an optional phase offset. These parameters fully determine the hazard's position on every turn.
- **Readable orientation:** the movement plane comes from the hazard's authored rotation. A circular hazard can stay flat on the board, or be rotated so the same loop runs vertically instead. The pattern must remain visually legible from the placed actor.
- **3D patrols:** linear patrol hazards may run horizontally or vertically through 3D space, but not diagonally. If an authored rotation points off-axis, the patrol must snap to the nearest readable axis-aligned direction.

These are intended primarily for single-player puzzle authoring, where exact replayability and solvable timing windows matter more than emergent randomness.

**Scope vs v1.** Phase 5 of §20 is the right insertion point: after replay save/load is working (puzzles ARE essentially replays with a win condition). Treat puzzle mode as a separate workstream from multiplayer — same engine, separate UI shell, separate save format wrapper.

---

## 23. Things to Explicitly Not Build in v1

To prevent scope creep, these are out of scope for the first playable build:

- Custom turtle skins or progression
- Match-making service (use Steam/EOS lobbies via SimpleLobbies)
- Ranking, ELO, leaderboards
- Single-player puzzle mode (see §22 — planned, but post-multiplayer)
- Maps beyond the procedural generator
- Replays as cloud-hosted shareables (local only in v1)
- AI opponents in *multiplayer* (puzzle-mode scripted opponents are different and live in §22)
- Spectators with delayed view (just real-time spectators)
- Mobile support
- Localization

Most of these are reasonable post-launch features. Note them in a `BACKLOG.md`; don't let them creep into v1.

---

## 24. Summary checklist for Claude Code

Before starting:

- [ ] Read this entire document
- [ ] Confirm Phase 1 has zero UE dependencies
- [ ] Confirm test harness runs without launching UE
- [ ] Confirm understanding of the §1 architectural principles

While building each phase:

- [ ] Every new function gets unit tests in the same commit
- [ ] Every change to the resolver re-runs the full fuzz harness
- [ ] No `FMath::Rand`, no float math, no unordered iteration in the simulation core
- [ ] Every tunable lives in §17's table or its config file, never hardcoded inline

When done with a phase:

- [ ] All tests green
- [ ] Determinism fuzz passes 1000 iterations
- [ ] No UE-specific types have leaked into `TurtleSim`
- [ ] Update this document if any decision changed

---

_End of spec. When in doubt, defer to the architectural principles in §1, then to determinism, then to player legibility._

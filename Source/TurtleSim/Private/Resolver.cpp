#include "Resolver.h"
#include "HunterAI.h"
#include "Orientation.h"

#include <algorithm>
#include <map>
#include <unordered_map>

namespace turtlesim {

namespace {

// ---- Internal mover record used by collision resolution. -------------------

enum class EMoverKind : std::uint8_t {
    Turtle = 0,
    Obstacle = 1,
};

struct Mover {
    EMoverKind   Kind = EMoverKind::Turtle;
    std::int32_t Id   = -1;        // TurtleId or ObstacleId.
    IntVec3      From;
    IntVec3      To;               // may be out-of-bounds (delivery).
    IntVec3      ForwardDir;       // direction of motion; (0,0,0) if not moving.
    Orientation  Facing;           // turtles only.
    bool         bMoving   = false;
    bool         bDelivery = false; // turtle attempted Forward off-grid.
};

// Composite key used to identify a mover deterministically.
struct EntityKey {
    EMoverKind   Kind;
    std::int32_t Id;
    bool operator==(const EntityKey& O) const { return Kind == O.Kind && Id == O.Id; }
};

struct EntityKeyHash {
    std::size_t operator()(const EntityKey& K) const noexcept {
        return (static_cast<std::size_t>(K.Kind) << 32) ^ static_cast<std::size_t>(K.Id);
    }
};

// Simple TickEvent constructors to keep call sites compact.
TickEvent EvTurtleTurned(TurtleId Id, Orientation From, Orientation To, IntVec3 At) {
    TickEvent E;
    E.Type = EEventType::TurtleTurned;
    E.PrimaryId = Id;
    E.FromPosition = At;
    E.ToPosition = At;
    E.FromOrientation = From;
    E.ToOrientation = To;
    return E;
}
TickEvent EvTurtleWaited(TurtleId Id, IntVec3 At) {
    TickEvent E;
    E.Type = EEventType::TurtleWaited;
    E.PrimaryId = Id;
    E.FromPosition = At;
    E.ToPosition = At;
    return E;
}
TickEvent EvTurtleBlocked(TurtleId Id, IntVec3 At, Orientation Facing) {
    TickEvent E;
    E.Type = EEventType::TurtleBlocked;
    E.PrimaryId = Id;
    E.FromPosition = At;
    E.ToPosition = At;
    E.FromOrientation = Facing;
    E.ToOrientation = Facing;
    return E;
}
TickEvent EvTurtleMoved(TurtleId Id, IntVec3 From, IntVec3 To, Orientation Facing) {
    TickEvent E;
    E.Type = EEventType::TurtleMoved;
    E.PrimaryId = Id;
    E.FromPosition = From;
    E.ToPosition = To;
    E.FromOrientation = Facing;
    E.ToOrientation = Facing;
    return E;
}
TickEvent EvTurtleDestroyed(TurtleId Id, IntVec3 At) {
    TickEvent E;
    E.Type = EEventType::TurtleDestroyed;
    E.PrimaryId = Id;
    E.FromPosition = At;
    E.ToPosition = At;
    return E;
}
TickEvent EvTurtleDelivered(TurtleId Id, IntVec3 From, std::int32_t Points) {
    TickEvent E;
    E.Type = EEventType::TurtleDelivered;
    E.PrimaryId = Id;
    E.FromPosition = From;
    E.ToPosition = From;
    E.PointsDelta = Points;
    return E;
}
TickEvent EvTurtleClaimed(TurtleId Id, ObjectiveId OId, IntVec3 At, std::int32_t Points) {
    TickEvent E;
    E.Type = EEventType::TurtleClaimedObjective;
    E.PrimaryId = Id;
    E.SecondaryId = OId;
    E.FromPosition = At;
    E.ToPosition = At;
    E.PointsDelta = Points;
    return E;
}
TickEvent EvObstacleMoved(ObstacleId Id, IntVec3 From, IntVec3 To) {
    TickEvent E;
    E.Type = EEventType::ObstacleMoved;
    E.PrimaryId = Id;
    E.FromPosition = From;
    E.ToPosition = To;
    return E;
}
TickEvent EvObstacleDestroyed(ObstacleId Id, IntVec3 At) {
    TickEvent E;
    E.Type = EEventType::ObstacleDestroyed;
    E.PrimaryId = Id;
    E.FromPosition = At;
    E.ToPosition = At;
    return E;
}

bool VectorsOpposite(const IntVec3& A, const IntVec3& B) {
    return A.X == -B.X && A.Y == -B.Y && A.Z == -B.Z && (A.X != 0 || A.Y != 0 || A.Z != 0);
}

IntVec3 RightVectorFor(const Orientation& O) {
    switch (O.Heading) {
        case EHeading::North: return IntVec3(-1, 0, 0);
        case EHeading::East:  return IntVec3(0, 1, 0);
        case EHeading::South: return IntVec3(1, 0, 0);
        case EHeading::West:  return IntVec3(0, -1, 0);
    }
    return IntVec3(0, 1, 0);
}

std::int32_t PositiveStepCount(const std::int32_t Value) {
    return std::max<std::int32_t>(1, Value);
}

std::int32_t PositiveStepDistance(const std::int32_t Value) {
    return std::max<std::int32_t>(1, Value);
}

std::int32_t WrapIndex(const std::int32_t Index, const std::int32_t Count) {
    if (Count <= 0) {
        return 0;
    }
    const std::int32_t Mod = Index % Count;
    return Mod >= 0 ? Mod : Mod + Count;
}

void AddUniquePoint(std::vector<IntVec3>& Points, const IntVec3& Point) {
    if (Points.empty() || Points.back() != Point) {
        Points.push_back(Point);
    }
}

std::vector<IntVec3> BuildWanderingPath(const Obstacle& O) {
    std::vector<IntVec3> Path;

    if (O.Pattern == EWanderingPattern::None) {
        Path.push_back(O.OriginPosition);
        return Path;
    }

    const std::int32_t StepDistance = PositiveStepDistance(O.StepDistance);
    const IntVec3 ForwardBase = O.PatternOrientation.ForwardVector();
    const IntVec3 RightBase = RightVectorFor(O.PatternOrientation);
    const IntVec3 Forward(ForwardBase.X * StepDistance, ForwardBase.Y * StepDistance, ForwardBase.Z * StepDistance);
    const IntVec3 Right(RightBase.X * StepDistance, RightBase.Y * StepDistance, RightBase.Z * StepDistance);

    if (O.Pattern == EWanderingPattern::LinearPatrol) {
        const std::int32_t ForwardSteps = PositiveStepCount(O.PrimaryStepCount);
        for (std::int32_t Step = 0; Step <= ForwardSteps; ++Step) {
            AddUniquePoint(Path, O.OriginPosition + IntVec3(Forward.X * Step, Forward.Y * Step, Forward.Z * Step));
        }
        for (std::int32_t Step = ForwardSteps - 1; Step >= 1; --Step) {
            AddUniquePoint(Path, O.OriginPosition + IntVec3(Forward.X * Step, Forward.Y * Step, Forward.Z * Step));
        }
    } else if (O.Pattern == EWanderingPattern::CircularLoop) {
        const std::int32_t ForwardSteps = PositiveStepCount(O.PrimaryStepCount);
        const std::int32_t RightSteps = PositiveStepCount(O.SecondaryStepCount);

        for (std::int32_t Step = 0; Step < RightSteps; ++Step) {
            AddUniquePoint(Path, O.OriginPosition + IntVec3(Right.X * Step, Right.Y * Step, Right.Z * Step));
        }
        for (std::int32_t Step = 0; Step < ForwardSteps; ++Step) {
            AddUniquePoint(Path, O.OriginPosition
                + IntVec3(Right.X * RightSteps, Right.Y * RightSteps, Right.Z * RightSteps)
                + IntVec3(Forward.X * Step, Forward.Y * Step, Forward.Z * Step));
        }
        for (std::int32_t Step = 0; Step < RightSteps; ++Step) {
            AddUniquePoint(Path, O.OriginPosition
                + IntVec3(Right.X * (RightSteps - Step), Right.Y * (RightSteps - Step), Right.Z * (RightSteps - Step))
                + IntVec3(Forward.X * ForwardSteps, Forward.Y * ForwardSteps, Forward.Z * ForwardSteps));
        }
        for (std::int32_t Step = 0; Step < ForwardSteps; ++Step) {
            AddUniquePoint(Path, O.OriginPosition
                + IntVec3(Forward.X * (ForwardSteps - Step), Forward.Y * (ForwardSteps - Step), Forward.Z * (ForwardSteps - Step)));
        }
    }

    if (Path.empty()) {
        Path.push_back(O.OriginPosition);
    }

    return Path;
}

IntVec3 GetWanderingPositionAtTurn(const Obstacle& O, const std::int32_t TurnNumber) {
    const std::vector<IntVec3> Path = BuildWanderingPath(O);
    if (Path.empty()) {
        return O.OriginPosition;
    }
    const std::int32_t PathIndex = WrapIndex(TurnNumber + O.PhaseOffset, static_cast<std::int32_t>(Path.size()));
    return Path[static_cast<std::size_t>(PathIndex)];
}

// Aggregate per-turtle plans from both turn plans, keyed by TurtleId.
// We don't trust Plan.Player here; ValidateTurnPlan is responsible for ownership.
const TurtlePlan* FindPlanFor(const TurnPlan& P, TurtleId Id) {
    for (const auto& TP : P.PerTurtle) if (TP.Turtle == Id) return &TP;
    return nullptr;
}

std::int32_t MaxPlanLength(const TurnPlan& A, const TurnPlan& B) {
    std::int32_t M = 0;
    for (const auto& TP : A.PerTurtle) M = std::max(M, static_cast<std::int32_t>(TP.Moves.size()));
    for (const auto& TP : B.PerTurtle) M = std::max(M, static_cast<std::int32_t>(TP.Moves.size()));
    return M;
}

} // anonymous namespace

TurnResult ResolveTurn(const BoardState& InitialState,
                       const TurnPlan& Plan0,
                       const TurnPlan& Plan1) {
    TurnResult Out;
    Out.FinalState = InitialState;
    BoardState& B = Out.FinalState;

    const std::int32_t TickCount = MaxPlanLength(Plan0, Plan1);
    if (TickCount == 0) {
        // No-ops on both sides means a do-nothing turn. Hunters do not act on empty turns
        // (consistent with "tick count = max plan length").
        ++B.TurnNumber;
        return Out;
    }

    for (std::int32_t Tick = 0; Tick < TickCount; ++Tick) {
        TickResult TR;
        TR.TickIndex = Tick;

        // --------------------------------------------------------------------
        // Phase 1: apply yaw/pitch immediately. These don't conflict with anything.
        // --------------------------------------------------------------------

        // Sort turtles by Id for deterministic event order.
        std::vector<std::int32_t> TurtleOrder;
        TurtleOrder.reserve(B.Turtles.size());
        for (const auto& T : B.Turtles) TurtleOrder.push_back(T.Id);
        std::sort(TurtleOrder.begin(), TurtleOrder.end());

        for (std::int32_t TId : TurtleOrder) {
            Turtle* T = B.FindTurtleMut(TId);
            if (!T || !T->bAlive || T->bDelivered) continue;

            const TurnPlan* Plan = (Plan0.PerTurtle.empty() ? nullptr : &Plan0);
            const TurtlePlan* TP = nullptr;
            for (const TurnPlan* P : {&Plan0, &Plan1}) {
                if (const TurtlePlan* Found = FindPlanFor(*P, TId)) { TP = Found; Plan = P; break; }
            }
            (void)Plan;
            if (!TP) continue; // no plan for this turtle.
            if (Tick >= static_cast<std::int32_t>(TP->Moves.size())) continue; // padded as Wait, no event emitted.

            const Move& M = TP->Moves[static_cast<std::size_t>(Tick)];
            const Orientation OldOri = T->Facing;
            switch (M.Kind) {
                case EMoveKind::YawLeft:
                    T->Facing.Heading = YawLeft(T->Facing.Heading);
                    TR.Events.push_back(EvTurtleTurned(TId, OldOri, T->Facing, T->Position));
                    break;
                case EMoveKind::YawRight:
                    T->Facing.Heading = YawRight(T->Facing.Heading);
                    TR.Events.push_back(EvTurtleTurned(TId, OldOri, T->Facing, T->Position));
                    break;
                case EMoveKind::PitchUp: {
                    auto Tr = TryPitchUp(T->Facing.Pitch);
                    if (Tr.bLegal) {
                        T->Facing.Pitch = Tr.New;
                        TR.Events.push_back(EvTurtleTurned(TId, OldOri, T->Facing, T->Position));
                    } else {
                        // ValidateTurnPlan should have rejected this. Be lenient: emit Wait.
                        TR.Events.push_back(EvTurtleWaited(TId, T->Position));
                    }
                    break;
                }
                case EMoveKind::PitchDown: {
                    auto Tr = TryPitchDown(T->Facing.Pitch);
                    if (Tr.bLegal) {
                        T->Facing.Pitch = Tr.New;
                        TR.Events.push_back(EvTurtleTurned(TId, OldOri, T->Facing, T->Position));
                    } else {
                        TR.Events.push_back(EvTurtleWaited(TId, T->Position));
                    }
                    break;
                }
                case EMoveKind::Wait:
                    TR.Events.push_back(EvTurtleWaited(TId, T->Position));
                    break;
                case EMoveKind::Forward:
                    // Spatial move handled in Phase 2 below.
                    break;
            }
        }

        // --------------------------------------------------------------------
        // Phase 2: collect spatial intents.
        // --------------------------------------------------------------------

        std::vector<Mover> Movers; // moving entities.
        // Stationary entities (not moving but can be hit). For turtles: those alive & not moving this tick.
        struct StationaryTurtle { TurtleId Id; IntVec3 At; Orientation Facing; };
        std::vector<StationaryTurtle> StationaryTurtles;

        for (std::int32_t TId : TurtleOrder) {
            Turtle* T = B.FindTurtleMut(TId);
            if (!T || !T->bAlive || T->bDelivered) continue;

            const TurtlePlan* TP = nullptr;
            for (const TurnPlan* P : {&Plan0, &Plan1}) {
                if (const TurtlePlan* Found = FindPlanFor(*P, TId)) { TP = Found; break; }
            }

            const bool HasForward =
                TP != nullptr
                && Tick < static_cast<std::int32_t>(TP->Moves.size())
                && TP->Moves[static_cast<std::size_t>(Tick)].Kind == EMoveKind::Forward;

            if (HasForward) {
                Mover M;
                M.Kind = EMoverKind::Turtle;
                M.Id = TId;
                M.From = T->Position;
                M.ForwardDir = T->Facing.ForwardVector();
                M.To = M.From + M.ForwardDir;
                M.Facing = T->Facing;
                M.bMoving = true;
                M.bDelivery = !B.IsInBounds(M.To);
                Movers.push_back(M);
            } else {
                StationaryTurtles.push_back({TId, T->Position, T->Facing});
            }
        }

        // Hunter intents.
        std::vector<HunterMoveDecision> HunterDecs = ComputeHunterMoves(B);
        for (const auto& HD : HunterDecs) {
            const Obstacle* H = B.FindObstacle(HD.Hunter);
            if (!H || !H->bAlive) continue;
            if (HD.bMoves) {
                Mover M;
                M.Kind = EMoverKind::Obstacle;
                M.Id = HD.Hunter;
                M.From = H->Position;
                M.To = HD.IntendedCell;
                M.ForwardDir = M.To - M.From;
                M.bMoving = true;
                Movers.push_back(M);
            }
        }

        if (Tick == 0) {
            std::vector<std::int32_t> ObstacleOrder;
            ObstacleOrder.reserve(B.Obstacles.size());
            for (const auto& O : B.Obstacles) {
                if (O.Kind == EObstacleKind::Wandering && O.bAlive) {
                    ObstacleOrder.push_back(O.Id);
                }
            }
            std::sort(ObstacleOrder.begin(), ObstacleOrder.end());

            for (std::int32_t ObstacleId : ObstacleOrder) {
                const Obstacle* O = B.FindObstacle(ObstacleId);
                if (!O || !O->bAlive) continue;

                const IntVec3 NextPosition = GetWanderingPositionAtTurn(*O, B.TurnNumber + 1);
                if (NextPosition == O->Position) {
                    continue;
                }

                Mover M;
                M.Kind = EMoverKind::Obstacle;
                M.Id = ObstacleId;
                M.From = O->Position;
                M.To = NextPosition;
                M.ForwardDir = M.To - M.From;
                M.bMoving = true;
                Movers.push_back(M);
            }
        }

        // Sort movers for deterministic processing: Turtles before Hunters, Id ascending.
        std::sort(Movers.begin(), Movers.end(), [](const Mover& A, const Mover& B2) {
            if (A.Kind != B2.Kind) return static_cast<std::uint8_t>(A.Kind) < static_cast<std::uint8_t>(B2.Kind);
            return A.Id < B2.Id;
        });

        // --------------------------------------------------------------------
        // Phase 3: collision resolution. See spec §10.
        // --------------------------------------------------------------------

        std::unordered_map<EntityKey, bool, EntityKeyHash> Destroyed; // true = destroyed this tick.
        auto IsDestroyed = [&](EMoverKind K, std::int32_t Id) {
            auto It = Destroyed.find({K, Id});
            return It != Destroyed.end() && It->second;
        };
        auto Destroy = [&](EMoverKind K, std::int32_t Id) {
            Destroyed[{K, Id}] = true;
        };

        // Track which static obstacles get hit (default: not destroyed; spec §10 Step C).
        // (Tunable — we keep statics indestructible per spec default §17.)

        // --- Step A: position swaps (head-on through an edge) ---
        for (std::size_t i = 0; i < Movers.size(); ++i) {
            const Mover& Mi = Movers[i];
            if (!Mi.bMoving || Mi.bDelivery) continue;
            for (std::size_t j = i + 1; j < Movers.size(); ++j) {
                const Mover& Mj = Movers[j];
                if (!Mj.bMoving || Mj.bDelivery) continue;
                if (Mi.From == Mj.To && Mi.To == Mj.From) {
                    Destroy(Mi.Kind, Mi.Id);
                    Destroy(Mj.Kind, Mj.Id);
                }
            }
        }

        // --- Step B: same-destination collisions ---
        // Group surviving movers by their To cell (ignoring deliveries — off-grid To never collides).
        {
            // Use ordered traversal: build cell->list-of-mover-indices in deterministic order.
            std::map<IntVec3, std::vector<std::size_t>> ByDest;
            for (std::size_t i = 0; i < Movers.size(); ++i) {
                const Mover& M = Movers[i];
                if (!M.bMoving || M.bDelivery) continue;
                if (IsDestroyed(M.Kind, M.Id)) continue;
                ByDest[M.To].push_back(i);
            }
            for (const auto& Pair : ByDest) {
                if (Pair.second.size() >= 2) {
                    for (std::size_t Idx : Pair.second) {
                        Destroy(Movers[Idx].Kind, Movers[Idx].Id);
                    }
                }
            }
        }

        // --- Step C: static obstacles ---
        // Build static-obstacle cell set.
        std::map<IntVec3, ObstacleId> StaticAt;
        for (const auto& O : B.Obstacles) {
            if (!O.bAlive) continue;
            if (O.Kind == EObstacleKind::Static) StaticAt[O.Position] = O.Id;
        }
        for (const auto& M : Movers) {
            if (!M.bMoving || M.bDelivery) continue;
            if (IsDestroyed(M.Kind, M.Id)) continue;
            auto It = StaticAt.find(M.To);
            if (It != StaticAt.end()) {
                Destroy(M.Kind, M.Id);
                // Static obstacle is unharmed (spec default).
            }
        }

        // --- Step D: hit a stationary turtle ---
        std::map<IntVec3, const StationaryTurtle*> StationaryAt;
        for (const auto& S : StationaryTurtles) StationaryAt[S.At] = &S;

        // Track stationary destruction & whether the mover survives at the cell.
        std::map<TurtleId, bool> StationaryDestroyed;
        for (const auto& M : Movers) {
            if (!M.bMoving || M.bDelivery) continue;
            if (IsDestroyed(M.Kind, M.Id)) continue;
            auto It = StationaryAt.find(M.To);
            if (It == StationaryAt.end()) continue;

            const StationaryTurtle* S = It->second;
            const IntVec3 SForward = S->Facing.ForwardVector();
            // "Opposite facings" => moving into a stationary that's facing back at us.
            // The mover's forward direction should be opposite to the stationary's facing forward.
            if (VectorsOpposite(M.ForwardDir, SForward)) {
                // Head-on: both destroyed.
                Destroy(M.Kind, M.Id);
                StationaryDestroyed[S->Id] = true;
            } else {
                // Rear/side hit: stationary destroyed, mover survives at cell.
                StationaryDestroyed[S->Id] = true;
            }
        }

        std::unordered_map<TurtleId, bool> DeliveryDestroyed;
        for (const auto& DeliveryMover : Movers) {
            if (!DeliveryMover.bDelivery || DeliveryMover.Kind != EMoverKind::Turtle) continue;
            for (const auto& M : Movers) {
                if (&M == &DeliveryMover) continue;
                if (!M.bMoving || M.bDelivery) continue;
                if (IsDestroyed(M.Kind, M.Id)) continue;
                if (M.To == DeliveryMover.From) {
                    DeliveryDestroyed[DeliveryMover.Id] = true;
                    break;
                }
            }
        }

        // --- Step E: apply survivors and emit events ---
        // Movers that survived go to their To. Destroyed movers stay at From and are killed.
        // Iterate movers in deterministic order.
        for (const auto& M : Movers) {
            if (M.bDelivery) {
                // Delivery handled in Phase 4 below — skip here.
                continue;
            }
            if (IsDestroyed(M.Kind, M.Id)) {
                if (M.Kind == EMoverKind::Turtle) {
                    Turtle* T = B.FindTurtleMut(M.Id);
                    if (T && T->bAlive) {
                        T->bAlive = false;
                        TR.Events.push_back(EvTurtleDestroyed(M.Id, M.From));
                    }
                } else {
                    Obstacle* O = B.FindObstacleMut(M.Id);
                    if (O && O->bAlive) {
                        O->bAlive = false;
                        TR.Events.push_back(EvObstacleDestroyed(M.Id, M.From));
                    }
                }
                continue;
            }
            // Survived; apply move.
            if (M.Kind == EMoverKind::Turtle) {
                Turtle* T = B.FindTurtleMut(M.Id);
                if (T && T->bAlive) {
                    TR.Events.push_back(EvTurtleMoved(M.Id, T->Position, M.To, T->Facing));
                    T->Position = M.To;
                }
            } else {
                Obstacle* O = B.FindObstacleMut(M.Id);
                if (O && O->bAlive) {
                    TR.Events.push_back(EvObstacleMoved(M.Id, O->Position, M.To));
                    O->Position = M.To;
                }
            }
        }

        // Apply stationary destructions.
        for (const auto& KV : StationaryDestroyed) {
            if (!KV.second) continue;
            Turtle* T = B.FindTurtleMut(KV.first);
            if (T && T->bAlive) {
                T->bAlive = false;
                TR.Events.push_back(EvTurtleDestroyed(KV.first, T->Position));
            }
        }

        // Emit "blocked" events for turtles whose Forward intent we silently treat as no-op.
        // (We don't currently produce those — Forward into walls is a delivery attempt only when
        // the cell is off-grid. There are no in-bounds wall cells; static obstacles destroy the
        // mover. No "blocked" path remains. Kept for future use.)

        // --------------------------------------------------------------------
        // Phase 4: deliveries.
        // --------------------------------------------------------------------
        for (const auto& M : Movers) {
            if (!M.bDelivery) continue;
            if (M.Kind != EMoverKind::Turtle) continue;
            Turtle* T = B.FindTurtleMut(M.Id);
            if (!T || !T->bAlive || T->bDelivered) continue;

            auto DestroyedIt = DeliveryDestroyed.find(M.Id);
            if (DestroyedIt != DeliveryDestroyed.end() && DestroyedIt->second) {
                T->bAlive = false;
                TR.Events.push_back(EvTurtleDestroyed(M.Id, M.From));
                continue;
            }

            // Per spec §9.1 step 5: deliverer must survive collision resolution.
            // Multiple deliverers remain valid because off-grid destinations do not collide.
            T->bDelivered = true;
            const std::int32_t Pts = T->CarriedPoints;
            if (T->Owner == 0) B.ScorePlayer0 += Pts;
            else if (T->Owner == 1) B.ScorePlayer1 += Pts;
            T->CarriedPoints = 0;
            TR.Events.push_back(EvTurtleDelivered(M.Id, M.From, Pts));
        }

        // --------------------------------------------------------------------
        // Phase 5: pickups. Surviving turtles ending on unclaimed objectives.
        // --------------------------------------------------------------------
        // Iterate turtles in Id order, objectives in Id order — deterministic.
        std::vector<std::int32_t> ObjOrder;
        for (const auto& O : B.Objectives) ObjOrder.push_back(O.Id);
        std::sort(ObjOrder.begin(), ObjOrder.end());

        for (std::int32_t TId : TurtleOrder) {
            Turtle* T = B.FindTurtleMut(TId);
            if (!T || !T->bAlive || T->bDelivered) continue;
            for (std::int32_t OId : ObjOrder) {
                Objective* O = B.FindObjectiveMut(OId);
                if (!O || O->bClaimed) continue;
                if (O->Position == T->Position) {
                    O->bClaimed = true;
                    T->CarriedPoints += O->PointValue;
                    TR.Events.push_back(EvTurtleClaimed(TId, OId, T->Position, O->PointValue));
                    // A turtle may stand on at most one objective per cell (we generate one per cell).
                }
            }
        }

        Out.Ticks.push_back(std::move(TR));
    }

    ++B.TurnNumber;
    return Out;
}

} // namespace turtlesim

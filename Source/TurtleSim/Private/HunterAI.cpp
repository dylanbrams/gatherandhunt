#include "HunterAI.h"

#include <algorithm>
#include <queue>
#include <vector>

namespace turtlesim {

// Six neighbor offsets in deterministic order: +X, -X, +Y, -Y, +Z, -Z.
// Fixed order is required for deterministic shortest-path tie-breaking.
static const IntVec3 kNeighbors[6] = {
    IntVec3(+1, 0, 0), IntVec3(-1, 0, 0),
    IntVec3(0, +1, 0), IntVec3(0, -1, 0),
    IntVec3(0, 0, +1), IntVec3(0, 0, -1),
};

static std::size_t CellIndex(const IntVec3& P, const IntVec3& Size) {
    return static_cast<std::size_t>(P.X)
         + static_cast<std::size_t>(P.Y) * Size.X
         + static_cast<std::size_t>(P.Z) * Size.X * Size.Y;
}

static HunterMoveDecision ComputeOne(const BoardState& State,
                                     const Obstacle& H,
                                     const std::vector<bool>& Blocked) {
    HunterMoveDecision D;
    D.Hunter = H.Id;
    D.IntendedCell = H.Position;
    D.bMoves = false;

    const IntVec3 Size = State.Size;
    const std::size_t N = static_cast<std::size_t>(Size.X * Size.Y * Size.Z);
    std::vector<int>     Parent(N, -1);     // index in cell-space; -1 = unvisited.
    std::vector<IntVec3> Cells(N);          // cell coords stored alongside.
    std::vector<int>     Distance(N, -1);

    std::queue<IntVec3> Q;
    const std::size_t StartIdx = CellIndex(H.Position, Size);
    Cells[StartIdx] = H.Position;
    Parent[StartIdx] = static_cast<int>(StartIdx); // self-parent marks origin.
    Distance[StartIdx] = 0;
    Q.push(H.Position);

    // Track best target so far (lowest TurtleId at min distance).
    int BestTurtleId = -1;
    int BestDistance = -1;
    IntVec3 BestCell{};

    // Build a per-cell map of TurtleId for quick lookup (deterministic; from sorted turtles by id later if needed).
    std::vector<int> TurtleIdAtCell(N, -1);
    for (const auto& T : State.Turtles) {
        if (!T.bAlive || T.bDelivered) continue;
        if (!State.IsInBounds(T.Position)) continue;
        const std::size_t Idx = CellIndex(T.Position, Size);
        // Deterministic: keep the lowest TurtleId if multiple share a cell (shouldn't happen post-collisions).
        if (TurtleIdAtCell[Idx] == -1 || T.Id < TurtleIdAtCell[Idx]) {
            TurtleIdAtCell[Idx] = T.Id;
        }
    }

    while (!Q.empty()) {
        const IntVec3 P = Q.front(); Q.pop();
        const std::size_t Idx = CellIndex(P, Size);
        const int Dist = Distance[Idx];

        // If this cell holds a turtle, it's a candidate.
        if (TurtleIdAtCell[Idx] != -1 && Idx != StartIdx) {
            const int TId = TurtleIdAtCell[Idx];
            if (BestDistance == -1
                || Dist < BestDistance
                || (Dist == BestDistance && TId < BestTurtleId)) {
                BestDistance = Dist;
                BestTurtleId = TId;
                BestCell = P;
            }
            // Don't expand past a turtle (turtles aren't blockers, but we also don't need
            // to traverse through them — and BFS guarantees shortest path so once we
            // see a candidate at distance D, any later same-distance candidate is found
            // via a different branch in the same BFS layer). Continue expanding to find
            // other equal-distance candidates with lower TurtleIds.
        }

        // If we already found a candidate at distance D, no need to explore neighbors at distance > D.
        if (BestDistance != -1 && Dist >= BestDistance) {
            continue;
        }

        for (const IntVec3& N6 : kNeighbors) {
            const IntVec3 Q2 = P + N6;
            if (!State.IsInBounds(Q2)) continue;
            const std::size_t QI = CellIndex(Q2, Size);
            if (Distance[QI] != -1) continue; // visited.
            if (Blocked[QI]) continue;
            Distance[QI] = Dist + 1;
            Parent[QI] = static_cast<int>(Idx);
            Cells[QI] = Q2;
            Q.push(Q2);
        }
    }

    if (BestTurtleId == -1) {
        return D; // No reachable turtle. Hunter holds.
    }

    // Walk parent chain back from BestCell to find the first step from H.Position.
    std::size_t Cur = CellIndex(BestCell, Size);
    while (Parent[Cur] != static_cast<int>(StartIdx) && Parent[Cur] != static_cast<int>(Cur)) {
        Cur = static_cast<std::size_t>(Parent[Cur]);
    }
    if (Parent[Cur] == static_cast<int>(Cur)) {
        // Best == start (shouldn't happen since we excluded StartIdx above).
        return D;
    }
    D.IntendedCell = Cells[Cur];
    D.bMoves = (D.IntendedCell != H.Position);
    return D;
}

std::vector<HunterMoveDecision> ComputeHunterMoves(const BoardState& State) {
    const IntVec3 Size = State.Size;
    const std::size_t N = static_cast<std::size_t>(Size.X * Size.Y * Size.Z);

    // Blocked = living obstacles (Static + Hunter). Turtles are NOT blockers (they're targets).
    // The hunter itself is treated as not-blocking its own start cell.
    std::vector<bool> Blocked(N, false);
    for (const auto& O : State.Obstacles) {
        if (!O.bAlive) continue;
        if (!State.IsInBounds(O.Position)) continue;
        Blocked[CellIndex(O.Position, Size)] = true;
    }

    // Deterministic: process hunters in ascending Id order.
    std::vector<const Obstacle*> Hunters;
    for (const auto& O : State.Obstacles) {
        if (O.Kind == EObstacleKind::Hunter && O.bAlive) Hunters.push_back(&O);
    }
    std::sort(Hunters.begin(), Hunters.end(),
              [](const Obstacle* A, const Obstacle* B) { return A->Id < B->Id; });

    std::vector<HunterMoveDecision> Out;
    Out.reserve(Hunters.size());
    for (const Obstacle* H : Hunters) {
        // Temporarily clear this hunter's own block so it can search outward.
        const std::size_t SelfIdx = CellIndex(H->Position, Size);
        const bool Saved = Blocked[SelfIdx];
        Blocked[SelfIdx] = false;
        Out.push_back(ComputeOne(State, *H, Blocked));
        Blocked[SelfIdx] = Saved;
    }
    return Out;
}

} // namespace turtlesim

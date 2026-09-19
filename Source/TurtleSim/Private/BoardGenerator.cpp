#include "BoardGenerator.h"
#include "RandomPCG.h"

#include <algorithm>
#include <queue>
#include <set>
#include <vector>

namespace turtlesim {

namespace {

IntVec3 MirrorX(const IntVec3& P, std::int32_t SizeX) {
    return IntVec3(SizeX - 1 - P.X, P.Y, P.Z);
}

bool OnMidplane(const IntVec3& P, std::int32_t SizeX) {
    return P.X == (SizeX - 1) / 2;
}

bool InHalf0(const IntVec3& P, std::int32_t SizeX) {
    return P.X < (SizeX - 1) / 2;
}

struct Occupancy {
    std::set<IntVec3> Cells; // cells with a turtle, obstacle, or objective.
    bool Has(const IntVec3& P) const { return Cells.find(P) != Cells.end(); }
    void Add(const IntVec3& P) { Cells.insert(P); }
};

bool TryPickHalf0Cell(PCG32& R, const IntVec3& Size, const Occupancy& Occ, IntVec3& Out, int Tries = 64) {
    const std::int32_t HalfX = (Size.X - 1) / 2;
    for (int i = 0; i < Tries; ++i) {
        IntVec3 P(R.RangeInt32(0, HalfX - 1),
                  R.RangeInt32(0, Size.Y - 1),
                  R.RangeInt32(0, Size.Z - 1));
        if (!Occ.Has(P) && !Occ.Has(MirrorX(P, Size.X))) {
            Out = P;
            return true;
        }
    }
    return false;
}

bool ReachableObjectivesFromSpawn(const BoardState& B, const Turtle& Spawn, std::int32_t MinReach) {
    // BFS from spawn, treating live obstacles as blocked. Count reachable objectives.
    const IntVec3 Size = B.Size;
    const std::size_t N = static_cast<std::size_t>(Size.X * Size.Y * Size.Z);
    auto Idx = [&](const IntVec3& P) {
        return static_cast<std::size_t>(P.X)
             + static_cast<std::size_t>(P.Y) * Size.X
             + static_cast<std::size_t>(P.Z) * Size.X * Size.Y;
    };
    std::vector<bool> Blocked(N, false);
    for (const auto& O : B.Obstacles) {
        if (O.bAlive && B.IsInBounds(O.Position)) Blocked[Idx(O.Position)] = true;
    }
    std::vector<bool> Visited(N, false);
    std::queue<IntVec3> Q;
    if (!B.IsInBounds(Spawn.Position)) return false;
    Q.push(Spawn.Position);
    Visited[Idx(Spawn.Position)] = true;
    int ObjectivesReached = 0;
    static const IntVec3 Neigh[6] = {
        IntVec3(+1, 0, 0), IntVec3(-1, 0, 0),
        IntVec3(0, +1, 0), IntVec3(0, -1, 0),
        IntVec3(0, 0, +1), IntVec3(0, 0, -1),
    };
    while (!Q.empty()) {
        IntVec3 P = Q.front(); Q.pop();
        for (const auto& Obj : B.Objectives) {
            if (Obj.Position == P) ++ObjectivesReached;
        }
        for (const auto& D : Neigh) {
            IntVec3 Q2 = P + D;
            if (!B.IsInBounds(Q2)) continue;
            std::size_t I = Idx(Q2);
            if (Visited[I] || Blocked[I]) continue;
            Visited[I] = true;
            Q.push(Q2);
        }
    }
    return ObjectivesReached >= MinReach;
}

bool TryGenerateOnce(const BoardGenParams& Params, std::uint32_t Seed, BoardState& Out) {
    PCG32 R(Seed);

    BoardState B;
    B.Size = Params.Size;
    B.Seed = Seed;

    Occupancy Occ;

    const std::int32_t HalfX = (Params.Size.X - 1) / 2;
    (void)HalfX;

    // 1) Spawns: place TurtlesPerPlayer turtles for player 0 along x = 0.
    //    Mirror onto player 1 at x = SizeX - 1.
    TurtleId NextTurtleId = 1;
    int Placed = 0;
    int Attempts = 0;
    while (Placed < Params.TurtlesPerPlayer && Attempts < 256) {
        ++Attempts;
        IntVec3 P(0, R.RangeInt32(0, Params.Size.Y - 1), R.RangeInt32(0, Params.Size.Z - 1));
        if (Occ.Has(P)) continue;
        Occ.Add(P);
        Occ.Add(MirrorX(P, Params.Size.X));

        Turtle T0;
        T0.Id = NextTurtleId++;
        T0.Owner = 0;
        T0.Position = P;
        T0.Facing = Orientation(EHeading::East, EPitch::Level);
        B.Turtles.push_back(T0);

        Turtle T1;
        T1.Id = NextTurtleId++;
        T1.Owner = 1;
        T1.Position = MirrorX(P, Params.Size.X);
        T1.Facing = Orientation(EHeading::West, EPitch::Level);
        B.Turtles.push_back(T1);

        ++Placed;
    }
    if (Placed < Params.TurtlesPerPlayer) return false;

    // 2) Static obstacles. Round count up to even via mirroring.
    ObstacleId NextObsId = 1;
    const std::int32_t StaticPerSide = Params.StaticObstacles / 2;
    for (std::int32_t i = 0; i < StaticPerSide; ++i) {
        IntVec3 P;
        if (!TryPickHalf0Cell(R, Params.Size, Occ, P)) return false;
        Occ.Add(P);
        Occ.Add(MirrorX(P, Params.Size.X));

        Obstacle O0; O0.Id = NextObsId++; O0.Kind = EObstacleKind::Static; O0.Position = P;
        Obstacle O1; O1.Id = NextObsId++; O1.Kind = EObstacleKind::Static;
        O1.Position = MirrorX(P, Params.Size.X);
        B.Obstacles.push_back(O0);
        B.Obstacles.push_back(O1);
    }

    // 3) Hunters.
    const std::int32_t HuntersPerSide = Params.Hunters / 2;
    for (std::int32_t i = 0; i < HuntersPerSide; ++i) {
        IntVec3 P;
        if (!TryPickHalf0Cell(R, Params.Size, Occ, P)) return false;
        Occ.Add(P);
        Occ.Add(MirrorX(P, Params.Size.X));

        Obstacle O0; O0.Id = NextObsId++; O0.Kind = EObstacleKind::Hunter; O0.Position = P;
        Obstacle O1; O1.Id = NextObsId++; O1.Kind = EObstacleKind::Hunter;
        O1.Position = MirrorX(P, Params.Size.X);
        B.Obstacles.push_back(O0);
        B.Obstacles.push_back(O1);
    }

    // 4) Objectives. Mirror-paired.
    ObjectiveId NextObjId = 1;
    for (std::int32_t Pts : Params.ObjectivePointPool) {
        IntVec3 P;
        if (!TryPickHalf0Cell(R, Params.Size, Occ, P)) return false;
        Occ.Add(P);
        Occ.Add(MirrorX(P, Params.Size.X));

        Objective J0; J0.Id = NextObjId++; J0.Position = P; J0.PointValue = Pts;
        Objective J1; J1.Id = NextObjId++; J1.Position = MirrorX(P, Params.Size.X); J1.PointValue = Pts;
        B.Objectives.push_back(J0);
        B.Objectives.push_back(J1);
    }

    // 5) Validate reachability: every spawn can reach at least 3 objectives.
    const std::int32_t MinReach = std::min<std::int32_t>(3, static_cast<std::int32_t>(B.Objectives.size()));
    for (const auto& T : B.Turtles) {
        if (!ReachableObjectivesFromSpawn(B, T, MinReach)) return false;
    }

    Out = std::move(B);
    return true;
}

} // anonymous namespace

BoardGenResult GenerateBoard(const BoardGenParams& Params) {
    BoardGenResult R;
    for (std::int32_t i = 0; i < Params.MaxRetries; ++i) {
        R.Retries = i;
        const std::uint32_t Seed = Params.Seed + static_cast<std::uint32_t>(i) * 2654435761u;
        if (TryGenerateOnce(Params, Seed, R.State)) {
            R.bOk = true;
            return R;
        }
    }
    R.bOk = false;
    return R;
}

bool ValidateMirrorSymmetry(const BoardState& B) {
    // For every owned-by-0 turtle, find a mirrored owned-by-1 counterpart.
    auto Mirror = [&](const IntVec3& P) { return IntVec3(B.Size.X - 1 - P.X, P.Y, P.Z); };

    auto MatchTurtle = [&](const Turtle& A) {
        for (const auto& C : B.Turtles) {
            if (C.Owner != 1) continue;
            if (C.Position == Mirror(A.Position)) return true;
        }
        return false;
    };
    for (const auto& T : B.Turtles) {
        if (T.Owner == 0 && !MatchTurtle(T)) return false;
    }

    auto MatchObstacle = [&](const Obstacle& A) {
        for (const auto& C : B.Obstacles) {
            if (C.Kind != A.Kind) continue;
            if (C.Position == Mirror(A.Position)) return true;
        }
        return false;
    };
    for (const auto& O : B.Obstacles) {
        if (InHalf0(O.Position, B.Size.X) || OnMidplane(O.Position, B.Size.X)) {
            if (!MatchObstacle(O)) return false;
        }
    }

    auto MatchObjective = [&](const Objective& A) {
        for (const auto& C : B.Objectives) {
            if (C.PointValue != A.PointValue) continue;
            if (C.Position == Mirror(A.Position)) return true;
        }
        return false;
    };
    for (const auto& J : B.Objectives) {
        if (InHalf0(J.Position, B.Size.X)) {
            if (!MatchObjective(J)) return false;
        }
    }
    return true;
}

} // namespace turtlesim

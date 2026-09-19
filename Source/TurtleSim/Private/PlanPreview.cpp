#include "PlanPreview.h"

namespace turtlesim {

std::vector<TrackStep> ExtractTurtleTrack(const BoardState& StartState,
                                          const TurnResult& Result,
                                          const TurtleId Turtle) {
    std::vector<TrackStep> Track;
    const turtlesim::Turtle* T = StartState.FindTurtle(Turtle);
    if (!T) {
        return Track;
    }

    TrackStep Start;
    Start.Position = T->Position;
    Start.Facing   = T->Facing;
    Track.push_back(Start);

    for (const TickResult& Tick : Result.Ticks) {
        // A claim is reported as its own event after the move that landed on the
        // objective; fold it into that tick's step rather than adding a new one.
        // Index, not pointer: Track may reallocate as steps are appended.
        std::size_t TickStep = static_cast<std::size_t>(-1);
        for (const TickEvent& E : Tick.Events) {
            if (E.PrimaryId != Turtle) continue;

            const TrackStep& Prev = Track.back();
            TrackStep Step;
            Step.TickIndex = Tick.TickIndex;
            Step.Position  = Prev.Position;
            Step.Facing    = Prev.Facing;

            switch (E.Type) {
                case EEventType::TurtleMoved:
                    Step.Kind     = ETrackStepKind::Moved;
                    Step.Position = E.ToPosition;
                    Step.Facing   = E.ToOrientation;
                    break;
                case EEventType::TurtleTurned:
                    Step.Kind   = ETrackStepKind::Turned;
                    Step.Facing = E.ToOrientation;
                    break;
                case EEventType::TurtleWaited:
                    Step.Kind = ETrackStepKind::Waited;
                    break;
                case EEventType::TurtleBlocked:
                    Step.Kind = ETrackStepKind::Blocked;
                    break;
                case EEventType::TurtleDestroyed:
                    // Destruction can follow a move on the same tick; replace that
                    // step so the path ends where the turtle actually died.
                    if (TickStep < Track.size()) {
                        Track[TickStep].Kind     = ETrackStepKind::Destroyed;
                        Track[TickStep].Position = E.ToPosition;
                        continue;
                    }
                    Step.Kind     = ETrackStepKind::Destroyed;
                    Step.Position = E.ToPosition;
                    break;
                case EEventType::TurtleDelivered:
                    Step.Kind        = ETrackStepKind::Delivered;
                    Step.Position    = E.FromPosition + Prev.Facing.ForwardVector();
                    Step.PointsDelta = E.PointsDelta;
                    break;
                case EEventType::TurtleClaimedObjective:
                    if (TickStep < Track.size()) {
                        Track[TickStep].bClaimedObjective = true;
                        Track[TickStep].PointsDelta += E.PointsDelta;
                    }
                    continue;
                default:
                    continue;
            }

            Track.push_back(Step);
            TickStep = Track.size() - 1;
        }
    }
    return Track;
}

} // namespace turtlesim

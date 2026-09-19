#include "Net/TurtleSerializer.h"

#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

namespace TurtleSimWire
{
	// Format magic + version. Bump kVersion on layout changes.
	static constexpr uint32 kMagic   = 0x54535357; // 'TSSW' little-endian.
	static constexpr uint16 kVersion = 1;

	// ---- Primitive helpers ------------------------------------------------

	static void WriteHeader(FArchive& Ar, uint8 PayloadKind)
	{
		uint32 Magic   = kMagic;
		uint16 Version = kVersion;
		Ar << Magic;
		Ar << Version;
		Ar << PayloadKind;
	}

	static bool ReadHeader(FArchive& Ar, uint8 ExpectedKind)
	{
		uint32 Magic = 0;
		uint16 Version = 0;
		uint8  Kind = 0;
		Ar << Magic;
		Ar << Version;
		Ar << Kind;
		if (Magic != kMagic) return false;
		if (Version != kVersion) return false;
		if (Kind != ExpectedKind) return false;
		return true;
	}

	static void WriteIntVec3(FArchive& Ar, const turtlesim::IntVec3& V)
	{
		int32 X = V.X, Y = V.Y, Z = V.Z;
		Ar << X; Ar << Y; Ar << Z;
	}

	static void ReadIntVec3(FArchive& Ar, turtlesim::IntVec3& V)
	{
		int32 X = 0, Y = 0, Z = 0;
		Ar << X; Ar << Y; Ar << Z;
		V.X = X; V.Y = Y; V.Z = Z;
	}

	static void WriteOrientation(FArchive& Ar, const turtlesim::Orientation& O)
	{
		uint8 H = static_cast<uint8>(O.Heading);
		uint8 P = static_cast<uint8>(O.Pitch);
		Ar << H; Ar << P;
	}

	static void ReadOrientation(FArchive& Ar, turtlesim::Orientation& O)
	{
		uint8 H = 0, P = 0;
		Ar << H; Ar << P;
		O.Heading = static_cast<turtlesim::EHeading>(H);
		O.Pitch   = static_cast<turtlesim::EPitch>(P);
	}

	// ---- BoardState -------------------------------------------------------

	void SerializeBoardState(TArray<uint8>& OutBytes, const turtlesim::BoardState& State)
	{
		FMemoryWriter Ar(OutBytes, /*bIsPersistent*/ true);
		WriteHeader(Ar, /*PayloadKind=BoardState*/ 1);

		WriteIntVec3(Ar, State.Size);
		int32 TurnNumber = State.TurnNumber; Ar << TurnNumber;
		int32 ScoreP0 = State.ScorePlayer0; Ar << ScoreP0;
		int32 ScoreP1 = State.ScorePlayer1; Ar << ScoreP1;
		uint32 Seed = State.Seed; Ar << Seed;

		int32 NumTurtles = static_cast<int32>(State.Turtles.size());
		Ar << NumTurtles;
		for (const auto& T : State.Turtles)
		{
			int32 Id = T.Id, Owner = T.Owner;
			Ar << Id; Ar << Owner;
			WriteIntVec3(Ar, T.Position);
			WriteOrientation(Ar, T.Facing);
			uint8 Alive = T.bAlive ? 1u : 0u;
			uint8 Delivered = T.bDelivered ? 1u : 0u;
			Ar << Alive; Ar << Delivered;
			int32 Carried = T.CarriedPoints; Ar << Carried;
		}

		int32 NumObstacles = static_cast<int32>(State.Obstacles.size());
		Ar << NumObstacles;
		for (const auto& O : State.Obstacles)
		{
			int32 Id = O.Id;
			uint8 Kind = static_cast<uint8>(O.Kind);
			uint8 Pattern = static_cast<uint8>(O.Pattern);
			Ar << Id; Ar << Kind; Ar << Pattern;
			WriteIntVec3(Ar, O.Position);
			WriteIntVec3(Ar, O.OriginPosition);
			WriteOrientation(Ar, O.PatternOrientation);
			int32 StepDistance = O.StepDistance;
			int32 PrimaryStepCount = O.PrimaryStepCount;
			int32 SecondaryStepCount = O.SecondaryStepCount;
			int32 PhaseOffset = O.PhaseOffset;
			Ar << StepDistance;
			Ar << PrimaryStepCount;
			Ar << SecondaryStepCount;
			Ar << PhaseOffset;
			uint8 Alive = O.bAlive ? 1u : 0u;
			Ar << Alive;
		}

		int32 NumObjectives = static_cast<int32>(State.Objectives.size());
		Ar << NumObjectives;
		for (const auto& J : State.Objectives)
		{
			int32 Id = J.Id; int32 Pts = J.PointValue;
			Ar << Id;
			WriteIntVec3(Ar, J.Position);
			Ar << Pts;
			uint8 Claimed = J.bClaimed ? 1u : 0u;
			Ar << Claimed;
		}
	}

	bool DeserializeBoardState(const TArray<uint8>& InBytes, turtlesim::BoardState& OutState)
	{
		FMemoryReader Ar(InBytes, /*bIsPersistent*/ true);
		if (!ReadHeader(Ar, 1)) return false;

		OutState = turtlesim::BoardState{};
		ReadIntVec3(Ar, OutState.Size);
		int32 TurnNumber = 0; Ar << TurnNumber; OutState.TurnNumber = TurnNumber;
		int32 ScoreP0 = 0; Ar << ScoreP0; OutState.ScorePlayer0 = ScoreP0;
		int32 ScoreP1 = 0; Ar << ScoreP1; OutState.ScorePlayer1 = ScoreP1;
		uint32 Seed = 0; Ar << Seed; OutState.Seed = Seed;

		int32 NumTurtles = 0; Ar << NumTurtles;
		OutState.Turtles.reserve(NumTurtles);
		for (int32 i = 0; i < NumTurtles; ++i)
		{
			turtlesim::Turtle T;
			int32 Id = 0, Owner = 0;
			Ar << Id; Ar << Owner;
			T.Id = Id; T.Owner = Owner;
			ReadIntVec3(Ar, T.Position);
			ReadOrientation(Ar, T.Facing);
			uint8 Alive = 0, Delivered = 0;
			Ar << Alive; Ar << Delivered;
			T.bAlive = Alive != 0; T.bDelivered = Delivered != 0;
			int32 Carried = 0; Ar << Carried; T.CarriedPoints = Carried;
			OutState.Turtles.push_back(T);
		}

		int32 NumObstacles = 0; Ar << NumObstacles;
		OutState.Obstacles.reserve(NumObstacles);
		for (int32 i = 0; i < NumObstacles; ++i)
		{
			turtlesim::Obstacle O;
			int32 Id = 0;
			uint8 Kind = 0;
			uint8 Pattern = 0;
			Ar << Id; Ar << Kind; Ar << Pattern;
			O.Id = Id;
			O.Kind = static_cast<turtlesim::EObstacleKind>(Kind);
			O.Pattern = static_cast<turtlesim::EWanderingPattern>(Pattern);
			ReadIntVec3(Ar, O.Position);
			ReadIntVec3(Ar, O.OriginPosition);
			ReadOrientation(Ar, O.PatternOrientation);
			int32 StepDistance = 1;
			int32 PrimaryStepCount = 1;
			int32 SecondaryStepCount = 1;
			int32 PhaseOffset = 0;
			Ar << StepDistance;
			Ar << PrimaryStepCount;
			Ar << SecondaryStepCount;
			Ar << PhaseOffset;
			O.StepDistance = StepDistance;
			O.PrimaryStepCount = PrimaryStepCount;
			O.SecondaryStepCount = SecondaryStepCount;
			O.PhaseOffset = PhaseOffset;
			uint8 Alive = 0; Ar << Alive; O.bAlive = Alive != 0;
			OutState.Obstacles.push_back(O);
		}

		int32 NumObjectives = 0; Ar << NumObjectives;
		OutState.Objectives.reserve(NumObjectives);
		for (int32 i = 0; i < NumObjectives; ++i)
		{
			turtlesim::Objective J;
			int32 Id = 0, Pts = 0;
			Ar << Id;
			ReadIntVec3(Ar, J.Position);
			Ar << Pts;
			J.Id = Id; J.PointValue = Pts;
			uint8 Claimed = 0; Ar << Claimed; J.bClaimed = Claimed != 0;
			OutState.Objectives.push_back(J);
		}

		return !Ar.IsError();
	}

	// ---- TurnPlan ---------------------------------------------------------

	void SerializeTurnPlan(TArray<uint8>& OutBytes, const turtlesim::TurnPlan& Plan)
	{
		FMemoryWriter Ar(OutBytes, true);
		WriteHeader(Ar, /*PayloadKind=TurnPlan*/ 2);

		int32 Player = Plan.Player; Ar << Player;
		int32 NumTurtles = static_cast<int32>(Plan.PerTurtle.size());
		Ar << NumTurtles;
		for (const auto& TP : Plan.PerTurtle)
		{
			int32 Id = TP.Turtle; Ar << Id;
			int32 NumMoves = static_cast<int32>(TP.Moves.size()); Ar << NumMoves;
			for (const auto& M : TP.Moves)
			{
				uint8 Kind = static_cast<uint8>(M.Kind); Ar << Kind;
			}
		}
	}

	bool DeserializeTurnPlan(const TArray<uint8>& InBytes, turtlesim::TurnPlan& OutPlan)
	{
		FMemoryReader Ar(InBytes, true);
		if (!ReadHeader(Ar, 2)) return false;

		OutPlan = turtlesim::TurnPlan{};
		int32 Player = 0; Ar << Player; OutPlan.Player = Player;
		int32 NumTurtles = 0; Ar << NumTurtles;
		OutPlan.PerTurtle.reserve(NumTurtles);
		for (int32 i = 0; i < NumTurtles; ++i)
		{
			turtlesim::TurtlePlan TP;
			int32 Id = 0; Ar << Id; TP.Turtle = Id;
			int32 NumMoves = 0; Ar << NumMoves;
			TP.Moves.reserve(NumMoves);
			for (int32 j = 0; j < NumMoves; ++j)
			{
				uint8 Kind = 0; Ar << Kind;
				TP.Moves.push_back(turtlesim::Move{ static_cast<turtlesim::EMoveKind>(Kind) });
			}
			OutPlan.PerTurtle.push_back(std::move(TP));
		}

		return !Ar.IsError();
	}

	// ---- TickEvent / TurnResult ------------------------------------------

	static void WriteTickEvent(FArchive& Ar, const turtlesim::TickEvent& E)
	{
		uint8 Type = static_cast<uint8>(E.Type); Ar << Type;
		int32 P = E.PrimaryId, S = E.SecondaryId, Pts = E.PointsDelta;
		Ar << P; Ar << S; Ar << Pts;
		WriteIntVec3(Ar, E.FromPosition);
		WriteIntVec3(Ar, E.ToPosition);
		WriteOrientation(Ar, E.FromOrientation);
		WriteOrientation(Ar, E.ToOrientation);
	}

	static void ReadTickEvent(FArchive& Ar, turtlesim::TickEvent& E)
	{
		uint8 Type = 0; Ar << Type; E.Type = static_cast<turtlesim::EEventType>(Type);
		int32 P = 0, S = 0, Pts = 0;
		Ar << P; Ar << S; Ar << Pts;
		E.PrimaryId = P; E.SecondaryId = S; E.PointsDelta = Pts;
		ReadIntVec3(Ar, E.FromPosition);
		ReadIntVec3(Ar, E.ToPosition);
		ReadOrientation(Ar, E.FromOrientation);
		ReadOrientation(Ar, E.ToOrientation);
	}

	void SerializeTurnResult(TArray<uint8>& OutBytes, const turtlesim::TurnResult& Result)
	{
		FMemoryWriter Ar(OutBytes, true);
		WriteHeader(Ar, /*PayloadKind=TurnResult*/ 3);

		int32 NumTicks = static_cast<int32>(Result.Ticks.size()); Ar << NumTicks;
		for (const auto& Tick : Result.Ticks)
		{
			int32 Idx = Tick.TickIndex; Ar << Idx;
			int32 NumEvents = static_cast<int32>(Tick.Events.size()); Ar << NumEvents;
			for (const auto& E : Tick.Events)
			{
				WriteTickEvent(Ar, E);
			}
		}

		// Final state inlined.
		TArray<uint8> StateBytes;
		SerializeBoardState(StateBytes, Result.FinalState);
		int32 StateSize = StateBytes.Num(); Ar << StateSize;
		Ar.Serialize(StateBytes.GetData(), StateSize);
	}

	bool DeserializeTurnResult(const TArray<uint8>& InBytes, turtlesim::TurnResult& OutResult)
	{
		FMemoryReader Ar(InBytes, true);
		if (!ReadHeader(Ar, 3)) return false;

		OutResult = turtlesim::TurnResult{};
		int32 NumTicks = 0; Ar << NumTicks;
		OutResult.Ticks.reserve(NumTicks);
		for (int32 i = 0; i < NumTicks; ++i)
		{
			turtlesim::TickResult TR;
			int32 Idx = 0; Ar << Idx; TR.TickIndex = Idx;
			int32 NumEvents = 0; Ar << NumEvents;
			TR.Events.reserve(NumEvents);
			for (int32 j = 0; j < NumEvents; ++j)
			{
				turtlesim::TickEvent E;
				ReadTickEvent(Ar, E);
				TR.Events.push_back(E);
			}
			OutResult.Ticks.push_back(std::move(TR));
		}

		int32 StateSize = 0; Ar << StateSize;
		TArray<uint8> StateBytes;
		StateBytes.SetNumUninitialized(StateSize);
		Ar.Serialize(StateBytes.GetData(), StateSize);
		if (!DeserializeBoardState(StateBytes, OutResult.FinalState)) return false;

		return !Ar.IsError();
	}
}

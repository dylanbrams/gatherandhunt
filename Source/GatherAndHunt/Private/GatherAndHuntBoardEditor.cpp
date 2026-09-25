#include "GatherAndHuntBoardEditor.h"

#include "GatherAndHuntPuzzleSession.h"
#include "ULevelSave.h"

namespace
{
	constexpr int32 MaxCellsPerAxis = 64;
	constexpr int32 MaxHazardSteps = 32;
	constexpr int32 MaxHazardStride = 8;

	// Yaw for each turtlesim heading (N = +Y, E = +X, S = -Y, W = -X).
	constexpr float HeadingYaw[4] = {90.0f, 0.0f, -90.0f, 180.0f};

	FRotator MakeRotation(const int32 Heading, const int32 Pitch)
	{
		const float PitchDegrees = Pitch == 2 ? 90.0f : (Pitch == 0 ? -90.0f : 0.0f);
		return FRotator(PitchDegrees, HeadingYaw[((Heading % 4) + 4) % 4], 0.0f);
	}

	void ApplyKind(FPieceSaveData& PieceData, const EGatherAndHuntBoardPieceKind Kind)
	{
		PieceData.MovementType = MovementTypeEnum::None;
		PieceData.PieceOwnerID = 0;
		PieceData.bScriptControlled = false;
		switch (Kind)
		{
		case EGatherAndHuntBoardPieceKind::Turtle:
			PieceData.PieceType = PieceTypeEnum::BP_PlayerOwnedPiece;
			break;
		case EGatherAndHuntBoardPieceKind::Objective:
			PieceData.PieceType = PieceTypeEnum::BP_Objective;
			break;
		case EGatherAndHuntBoardPieceKind::GoldObjective:
			PieceData.PieceType = PieceTypeEnum::BP_GoldObjective;
			break;
		case EGatherAndHuntBoardPieceKind::Block:
			PieceData.PieceType = PieceTypeEnum::BP_Hazard;
			break;
		case EGatherAndHuntBoardPieceKind::Hunter:
			PieceData.PieceType = PieceTypeEnum::BP_Hazard;
			PieceData.MovementType = MovementTypeEnum::BP_ChaseNearest;
			break;
		case EGatherAndHuntBoardPieceKind::PatrolHazard:
			PieceData.PieceType = PieceTypeEnum::BP_LinearPatrolHazard;
			PieceData.MovementType = MovementTypeEnum::BP_AbsoluteMoveLinearPatrol;
			break;
		case EGatherAndHuntBoardPieceKind::LoopHazard:
			PieceData.PieceType = PieceTypeEnum::BP_CircularHazard;
			PieceData.MovementType = MovementTypeEnum::BP_AbsoluteMoveCircle;
			break;
		}
	}

	FIntVector ClampCell(const FIntVector& Cell, const FIntVector& Count)
	{
		return FIntVector(
			FMath::Clamp(Cell.X, 0, Count.X - 1),
			FMath::Clamp(Cell.Y, 0, Count.Y - 1),
			FMath::Clamp(Cell.Z, 0, Count.Z - 1));
	}
}

EGatherAndHuntBoardPieceKind UGatherAndHuntBoardEditor::GetPieceKind(const FPieceSaveData& PieceData)
{
	switch (PieceData.PieceType)
	{
	case PieceTypeEnum::BP_PlayerOwnedPiece: return EGatherAndHuntBoardPieceKind::Turtle;
	case PieceTypeEnum::BP_Objective: return EGatherAndHuntBoardPieceKind::Objective;
	case PieceTypeEnum::BP_GoldObjective: return EGatherAndHuntBoardPieceKind::GoldObjective;
	default: break;
	}
	// Same precedence as the session's conversion to sim obstacles.
	if (PieceData.IsCircularHazard()) return EGatherAndHuntBoardPieceKind::LoopHazard;
	if (PieceData.IsLinearPatrolHazard()) return EGatherAndHuntBoardPieceKind::PatrolHazard;
	if (PieceData.MovementType == MovementTypeEnum::BP_ChaseNearest || PieceData.MovementType == MovementTypeEnum::BP_ChaseTarget)
	{
		return EGatherAndHuntBoardPieceKind::Hunter;
	}
	return EGatherAndHuntBoardPieceKind::Block;
}

void UGatherAndHuntBoardEditor::BindToSession(UGatherAndHuntPuzzleSession* InSession)
{
	Session = InSession;
	SyncFromSession();
}

void UGatherAndHuntBoardEditor::SyncFromSession()
{
	UndoStack.Reset();
	RedoStack.Reset();
	const ULevelSave* const Source = Session != nullptr && Session->IsPuzzleLoaded() ? Session->GetLevelSave() : nullptr;
	Working = Source != nullptr ? DuplicateObject<ULevelSave>(Source, this) : nullptr;
	if (Working != nullptr)
	{
		Session->ResetToInitialState();
		Cursor = ClampCell(Cursor, GetCellCount());
	}
	OnChanged.Broadcast(true);
}

bool UGatherAndHuntBoardEditor::NewPuzzle(const FString& PuzzleName, const FIntVector CellCount)
{
	if (Session == nullptr)
	{
		return false;
	}
	ULevelSave* const Fresh = UGatherAndHuntLevelEditorLibrary::CreateEmptyLevelSave(this, PuzzleName, 0, FVector::OneVector);
	Fresh->GridCellSize = 100.0f;
	Working = Fresh;
	UndoStack.Reset();
	RedoStack.Reset();
	Cursor = FIntVector::ZeroValue;
	// ResizeBoard would record an undo step; set the size directly instead.
	const FIntVector Count(
		FMath::Clamp(CellCount.X, 1, MaxCellsPerAxis),
		FMath::Clamp(CellCount.Y, 1, MaxCellsPerAxis),
		FMath::Clamp(CellCount.Z, 1, MaxCellsPerAxis));
	Working->GridSize = FVector(Count - FIntVector(1)) * Working->GridCellSize;
	CommitEdit();
	return true;
}

// --- Geometry -----------------------------------------------------------------

float UGatherAndHuntBoardEditor::GetCellSize() const
{
	return Working != nullptr && Working->GridCellSize > 0.0f ? Working->GridCellSize : 100.0f;
}

FIntVector UGatherAndHuntBoardEditor::ToCell(const FVector& Position) const
{
	const float CellSize = GetCellSize();
	return FIntVector(
		FMath::RoundToInt(Position.X / CellSize),
		FMath::RoundToInt(Position.Y / CellSize),
		FMath::RoundToInt(Position.Z / CellSize));
}

FVector UGatherAndHuntBoardEditor::ToPosition(const FIntVector& Cell) const
{
	return FVector(Cell) * GetCellSize();
}

FIntVector UGatherAndHuntBoardEditor::GetCellCount() const
{
	if (Working == nullptr)
	{
		return FIntVector(1);
	}
	// Matches the session's conversion: GridSize is the far corner's position.
	const float CellSize = GetCellSize();
	return FIntVector(
		FMath::Max(1, FMath::RoundToInt(Working->GridSize.X / CellSize) + 1),
		FMath::Max(1, FMath::RoundToInt(Working->GridSize.Y / CellSize) + 1),
		FMath::Max(1, FMath::RoundToInt(Working->GridSize.Z / CellSize) + 1));
}

bool UGatherAndHuntBoardEditor::IsInside(const FIntVector& Cell) const
{
	const FIntVector Count = GetCellCount();
	return Cell.X >= 0 && Cell.Y >= 0 && Cell.Z >= 0 && Cell.X < Count.X && Cell.Y < Count.Y && Cell.Z < Count.Z;
}

FPieceSaveData* UGatherAndHuntBoardEditor::FindPieceAt(const FIntVector& Cell)
{
	if (Working == nullptr)
	{
		return nullptr;
	}
	for (FPieceSaveData& PieceData : Working->PieceSaveDataArray)
	{
		if (ToCell(PieceData.Position) == Cell)
		{
			return &PieceData;
		}
	}
	return nullptr;
}

const FPieceSaveData* UGatherAndHuntBoardEditor::FindPieceAt(const FIntVector& Cell) const
{
	return const_cast<UGatherAndHuntBoardEditor*>(this)->FindPieceAt(Cell);
}

FGatherAndHuntBoardEditorPiece UGatherAndHuntBoardEditor::MakeView(const FPieceSaveData& PieceData) const
{
	FGatherAndHuntBoardEditorPiece View;
	View.PieceId = PieceData.PieceSaveID;
	View.Kind = GetPieceKind(PieceData);
	View.Cell = ToCell(PieceData.Position);
	const turtlesim::Orientation Facing = UGatherAndHuntPuzzleSession::ConvertWorldRotationToOrientation(PieceData.Rotation);
	View.Heading = static_cast<int32>(Facing.Heading);
	View.Pitch = static_cast<int32>(Facing.Pitch);
	View.Role = UGatherAndHuntLevelEditorLibrary::GetTurtleRole(PieceData);
	View.Length = FMath::Max(1, PieceData.HazardPrimaryStepCount);
	View.Width = FMath::Max(1, PieceData.HazardSecondaryStepCount);
	View.Stride = FMath::Max(1, FMath::RoundToInt(PieceData.HazardStepDistance / GetCellSize()));
	View.Phase = PieceData.HazardPhaseOffset;
	return View;
}

// --- Cursor -------------------------------------------------------------------

void UGatherAndHuntBoardEditor::SetCursor(const FIntVector Cell)
{
	const FIntVector Clamped = ClampCell(Cell, GetCellCount());
	if (Clamped != Cursor)
	{
		Cursor = Clamped;
		OnChanged.Broadcast(false);
	}
}

void UGatherAndHuntBoardEditor::MoveCursor(const FIntVector Delta)
{
	SetCursor(Cursor + Delta);
}

TArray<FGatherAndHuntBoardEditorPiece> UGatherAndHuntBoardEditor::GetPieces() const
{
	TArray<FGatherAndHuntBoardEditorPiece> Pieces;
	if (Working != nullptr)
	{
		for (const FPieceSaveData& PieceData : Working->PieceSaveDataArray)
		{
			Pieces.Add(MakeView(PieceData));
		}
	}
	return Pieces;
}

bool UGatherAndHuntBoardEditor::GetSelectedPiece(FGatherAndHuntBoardEditorPiece& OutPiece) const
{
	if (const FPieceSaveData* const PieceData = FindPieceAt(Cursor))
	{
		OutPiece = MakeView(*PieceData);
		return true;
	}
	return false;
}

// --- Piece edits --------------------------------------------------------------

bool UGatherAndHuntBoardEditor::PlacePiece(const EGatherAndHuntBoardPieceKind Kind)
{
	if (Working == nullptr || !IsInside(Cursor) || FindPieceAt(Cursor) != nullptr)
	{
		return false;
	}

	BeginEdit();
	FPieceSaveData PieceData;
	PieceData.PieceSaveID = Working->GetNextAvailablePieceSaveID();
	PieceData.Position = ToPosition(Cursor);
	PieceData.Rotation = MakeRotation(1, 1);
	ApplyKind(PieceData, Kind);
	PieceData.HazardStepDistance = GetCellSize();
	PieceData.HazardPrimaryStepCount = 2;
	PieceData.HazardSecondaryStepCount = 2;
	PieceData.HazardPhaseOffset = 0;
	Working->PieceSaveDataArray.Add(PieceData);
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::RemoveSelectedPiece()
{
	const FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr)
	{
		return false;
	}
	const int32 PieceId = PieceData->PieceSaveID;
	BeginEdit();
	UGatherAndHuntLevelEditorLibrary::RemovePieceDataByID(Working, PieceId);
	RemoveScriptedMovesFor(PieceId);
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::MoveSelectedPiece(const FIntVector Delta)
{
	const FIntVector Target = Cursor + Delta;
	if (FindPieceAt(Cursor) == nullptr || !IsInside(Target) || FindPieceAt(Target) != nullptr)
	{
		return false;
	}
	BeginEdit();
	FindPieceAt(Cursor)->Position = ToPosition(Target);
	Cursor = Target;
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::RotateSelectedPiece(const int32 Direction)
{
	FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr || Direction == 0)
	{
		return false;
	}
	const EGatherAndHuntBoardPieceKind Kind = GetPieceKind(*PieceData);
	if (Kind != EGatherAndHuntBoardPieceKind::Turtle && !IsMovingHazard(Kind))
	{
		return false; // Nothing else has a meaningful facing.
	}
	const FGatherAndHuntBoardEditorPiece View = MakeView(*PieceData);
	BeginEdit();
	// Headings run clockwise (N, E, S, W), so turning right is +1.
	FindPieceAt(Cursor)->Rotation = MakeRotation(View.Heading + (Direction > 0 ? 1 : -1), View.Pitch);
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::PitchSelectedPiece(const int32 Direction)
{
	const FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr || GetPieceKind(*PieceData) != EGatherAndHuntBoardPieceKind::Turtle)
	{
		return false;
	}
	const FGatherAndHuntBoardEditorPiece View = MakeView(*PieceData);
	const int32 NewPitch = FMath::Clamp(View.Pitch + (Direction > 0 ? 1 : -1), 0, 2);
	if (NewPitch == View.Pitch)
	{
		return false;
	}
	BeginEdit();
	FindPieceAt(Cursor)->Rotation = MakeRotation(View.Heading, NewPitch);
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::SetSelectedTurtleRole(const EGatherAndHuntTurtleRole Role)
{
	const FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr || GetPieceKind(*PieceData) != EGatherAndHuntBoardPieceKind::Turtle
		|| UGatherAndHuntLevelEditorLibrary::GetTurtleRole(*PieceData) == Role)
	{
		return false;
	}
	const int32 PieceId = PieceData->PieceSaveID;
	BeginEdit();
	UGatherAndHuntLevelEditorLibrary::SetTurtleRole(*FindPieceAt(Cursor), Role);
	if (Role == EGatherAndHuntTurtleRole::Player)
	{
		// The player plans it now; scripted moves for it would be ignored.
		RemoveScriptedMovesFor(PieceId);
	}
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::CycleSelectedTurtleRole()
{
	const FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr || GetPieceKind(*PieceData) != EGatherAndHuntBoardPieceKind::Turtle)
	{
		return false;
	}
	switch (UGatherAndHuntLevelEditorLibrary::GetTurtleRole(*PieceData))
	{
	case EGatherAndHuntTurtleRole::Player: return SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Ally);
	case EGatherAndHuntTurtleRole::Ally: return SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Opponent);
	default: return SetSelectedTurtleRole(EGatherAndHuntTurtleRole::Player);
	}
}

bool UGatherAndHuntBoardEditor::AdjustSelectedHazard(const EGatherAndHuntHazardParam Param, const int32 Delta)
{
	const FPieceSaveData* const PieceData = FindPieceAt(Cursor);
	if (PieceData == nullptr || Delta == 0)
	{
		return false;
	}
	const EGatherAndHuntBoardPieceKind Kind = GetPieceKind(*PieceData);
	if (!IsMovingHazard(Kind) || (Param == EGatherAndHuntHazardParam::Width && Kind != EGatherAndHuntBoardPieceKind::LoopHazard))
	{
		return false;
	}

	FPieceSaveData Edited = *PieceData;
	switch (Param)
	{
	case EGatherAndHuntHazardParam::Length:
		Edited.HazardPrimaryStepCount = FMath::Clamp(Edited.HazardPrimaryStepCount + Delta, 1, MaxHazardSteps);
		break;
	case EGatherAndHuntHazardParam::Width:
		Edited.HazardSecondaryStepCount = FMath::Clamp(Edited.HazardSecondaryStepCount + Delta, 1, MaxHazardSteps);
		break;
	case EGatherAndHuntHazardParam::Stride:
	{
		const int32 Stride = FMath::Max(1, FMath::RoundToInt(Edited.HazardStepDistance / GetCellSize()));
		Edited.HazardStepDistance = FMath::Clamp(Stride + Delta, 1, MaxHazardStride) * GetCellSize();
		break;
	}
	case EGatherAndHuntHazardParam::Phase:
	{
		const int32 CycleLength = FMath::Max(1, Edited.GetDeterministicHazardPathLength());
		Edited.HazardPhaseOffset = ((Edited.HazardPhaseOffset + Delta) % CycleLength + CycleLength) % CycleLength;
		break;
	}
	}

	if (Edited.HazardPrimaryStepCount == PieceData->HazardPrimaryStepCount
		&& Edited.HazardSecondaryStepCount == PieceData->HazardSecondaryStepCount
		&& Edited.HazardStepDistance == PieceData->HazardStepDistance
		&& Edited.HazardPhaseOffset == PieceData->HazardPhaseOffset)
	{
		return false; // Already at a limit.
	}
	BeginEdit();
	*FindPieceAt(Cursor) = Edited;
	CommitEdit();
	return true;
}

TArray<FIntVector> UGatherAndHuntBoardEditor::GetHazardPath(const int32 PieceId) const
{
	TArray<FIntVector> Path;
	if (Working == nullptr)
	{
		return Path;
	}
	for (const FPieceSaveData& PieceData : Working->PieceSaveDataArray)
	{
		if (PieceData.PieceSaveID != PieceId || !PieceData.IsDeterministicHazard())
		{
			continue;
		}
		const int32 CycleLength = FMath::Max(1, PieceData.GetDeterministicHazardPathLength());
		// One full cycle, ending back where it started.
		for (int32 Turn = 0; Turn <= CycleLength; ++Turn)
		{
			Path.Add(ToCell(PieceData.GetDeterministicHazardPositionAtTurn(Turn)));
		}
		break;
	}
	return Path;
}

// --- Metadata -----------------------------------------------------------------

FString UGatherAndHuntBoardEditor::GetPuzzleName() const { return Working != nullptr ? Working->LevelName : FString(); }
FString UGatherAndHuntBoardEditor::GetIntroText() const { return Working != nullptr ? Working->IntroText : FString(); }
int32 UGatherAndHuntBoardEditor::GetTargetScore() const { return Working != nullptr ? Working->TargetScore : 0; }
int32 UGatherAndHuntBoardEditor::GetTargetTurnCount() const { return Working != nullptr ? Working->TargetTurnCount : 0; }
int32 UGatherAndHuntBoardEditor::GetTargetObjectiveCount() const { return Working != nullptr ? Working->TargetObjectiveCount : 0; }
int32 UGatherAndHuntBoardEditor::GetPuzzleAct() const { return Working != nullptr ? Working->PuzzleAct : 1; }
int32 UGatherAndHuntBoardEditor::GetPuzzleOrder() const { return Working != nullptr ? Working->PuzzleOrder : 0; }

void UGatherAndHuntBoardEditor::SetPuzzleName(const FString& Name)
{
	const FString Trimmed = Name.TrimStartAndEnd();
	if (Working != nullptr && Working->LevelName != Trimmed)
	{
		BeginEdit();
		Working->LevelName = Trimmed;
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetIntroText(const FString& Text)
{
	if (Working != nullptr && Working->IntroText != Text)
	{
		BeginEdit();
		Working->IntroText = Text;
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetTargetScore(const int32 Value)
{
	if (Working != nullptr && Working->TargetScore != FMath::Max(0, Value))
	{
		BeginEdit();
		Working->TargetScore = FMath::Max(0, Value);
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetTargetTurnCount(const int32 Value)
{
	if (Working != nullptr && Working->TargetTurnCount != FMath::Max(0, Value))
	{
		BeginEdit();
		Working->TargetTurnCount = FMath::Max(0, Value);
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetTargetObjectiveCount(const int32 Value)
{
	if (Working != nullptr && Working->TargetObjectiveCount != FMath::Max(0, Value))
	{
		BeginEdit();
		Working->TargetObjectiveCount = FMath::Max(0, Value);
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetPuzzleActAndOrder(const int32 Act, const int32 Order)
{
	if (Working != nullptr && (Working->PuzzleAct != FMath::Max(1, Act) || Working->PuzzleOrder != FMath::Max(0, Order)))
	{
		BeginEdit();
		Working->PuzzleAct = FMath::Max(1, Act);
		Working->PuzzleOrder = FMath::Max(0, Order);
		CommitEdit();
	}
}

// --- Puzzle rules ---------------------------------------------------------------

EGatherAndHuntPuzzleGoal UGatherAndHuntBoardEditor::GetGoal() const { return Working != nullptr ? Working->Goal : EGatherAndHuntPuzzleGoal::Deliver; }
bool UGatherAndHuntBoardEditor::GetShowGround() const { return Working != nullptr && Working->bShowGround; }
int32 UGatherAndHuntBoardEditor::GetMaxTurtlesMovedPerTurn() const { return Working != nullptr ? Working->MaxTurtlesMovedPerTurn : 0; }

bool UGatherAndHuntBoardEditor::IsMoveAllowed(const EGatherAndHuntPuzzleMoveKind MoveKind) const
{
	return IsGatherAndHuntMoveAllowed(Working != nullptr ? Working->AllowedMoveMask : GatherAndHuntAllMovesMask, MoveKind);
}

void UGatherAndHuntBoardEditor::SetGoal(const EGatherAndHuntPuzzleGoal Goal)
{
	if (Working != nullptr && Working->Goal != Goal)
	{
		BeginEdit();
		Working->Goal = Goal;
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetMoveAllowed(const EGatherAndHuntPuzzleMoveKind MoveKind, const bool bAllowed)
{
	if (Working == nullptr || IsMoveAllowed(MoveKind) == bAllowed)
	{
		return;
	}
	BeginEdit();
	const int32 Bit = 1 << static_cast<int32>(MoveKind);
	Working->AllowedMoveMask = bAllowed ? (Working->AllowedMoveMask | Bit) : (Working->AllowedMoveMask & ~Bit);
	CommitEdit();
}

void UGatherAndHuntBoardEditor::SetShowGround(const bool bShow)
{
	if (Working != nullptr && Working->bShowGround != bShow)
	{
		BeginEdit();
		Working->bShowGround = bShow;
		CommitEdit();
	}
}

void UGatherAndHuntBoardEditor::SetMaxTurtlesMovedPerTurn(const int32 Value)
{
	if (Working != nullptr && Working->MaxTurtlesMovedPerTurn != FMath::Max(0, Value))
	{
		BeginEdit();
		Working->MaxTurtlesMovedPerTurn = FMath::Max(0, Value);
		CommitEdit();
	}
}

bool UGatherAndHuntBoardEditor::ResizeBoard(const FIntVector CellCount)
{
	if (Working == nullptr)
	{
		return false;
	}
	const FIntVector Count(
		FMath::Clamp(CellCount.X, 1, MaxCellsPerAxis),
		FMath::Clamp(CellCount.Y, 1, MaxCellsPerAxis),
		FMath::Clamp(CellCount.Z, 1, MaxCellsPerAxis));
	if (Count == GetCellCount())
	{
		return false;
	}
	for (const FPieceSaveData& PieceData : Working->PieceSaveDataArray)
	{
		const FIntVector Cell = ToCell(PieceData.Position);
		if (Cell.X >= Count.X || Cell.Y >= Count.Y || Cell.Z >= Count.Z)
		{
			return false;
		}
	}
	BeginEdit();
	Working->GridSize = FVector(Count - FIntVector(1)) * GetCellSize();
	Cursor = ClampCell(Cursor, Count);
	CommitEdit();
	return true;
}

// --- Validation ---------------------------------------------------------------

TArray<FLevelEditorValidationIssue> UGatherAndHuntBoardEditor::GetIssues() const
{
	TArray<FLevelEditorValidationIssue> Issues;
	if (Working == nullptr)
	{
		return Issues;
	}
	UGatherAndHuntLevelEditorLibrary::ValidateLevelSave(Working, Issues);

	auto AddIssue = [&Issues](const FString& Message, const int32 PieceId, const bool bIsError)
	{
		FLevelEditorValidationIssue& Issue = Issues.AddDefaulted_GetRef();
		Issue.Message = Message;
		Issue.PieceSaveID = PieceId;
		Issue.bIsError = bIsError;
	};

	TMap<FIntVector, int32> Occupied;
	bool bHasPlayerTurtle = false;
	for (const FPieceSaveData& PieceData : Working->PieceSaveDataArray)
	{
		const FIntVector Cell = ToCell(PieceData.Position);
		if (const int32* const Other = Occupied.Find(Cell))
		{
			AddIssue(FString::Printf(TEXT("Shares cell (%d,%d,%d) with piece %d."), Cell.X, Cell.Y, Cell.Z, *Other), PieceData.PieceSaveID, true);
		}
		else
		{
			Occupied.Add(Cell, PieceData.PieceSaveID);
		}

		const EGatherAndHuntBoardPieceKind Kind = GetPieceKind(PieceData);
		if (Kind == EGatherAndHuntBoardPieceKind::Turtle && UGatherAndHuntLevelEditorLibrary::GetTurtleRole(PieceData) == EGatherAndHuntTurtleRole::Player)
		{
			bHasPlayerTurtle = true;
		}
		if (IsMovingHazard(Kind))
		{
			for (const FIntVector& Step : GetHazardPath(PieceData.PieceSaveID))
			{
				if (!IsInside(Step))
				{
					AddIssue(TEXT("Hazard path leaves the board."), PieceData.PieceSaveID, false);
					break;
				}
			}
		}
	}
	if (!bHasPlayerTurtle)
	{
		AddIssue(TEXT("No player turtle: the puzzle can't be played."), INDEX_NONE, true);
	}
	if ((Working->AllowedMoveMask & GatherAndHuntAllMovesMask) == 0)
	{
		AddIssue(TEXT("No moves are allowed: the player can't do anything."), INDEX_NONE, true);
	}
	return Issues;
}

// --- Undo ---------------------------------------------------------------------

void UGatherAndHuntBoardEditor::BeginEdit()
{
	UndoStack.Add(DuplicateObject<ULevelSave>(Working, this));
	if (UndoStack.Num() > FMath::Max(1, MaxUndoSteps))
	{
		UndoStack.RemoveAt(0);
	}
	RedoStack.Reset();
}

void UGatherAndHuntBoardEditor::CommitEdit()
{
	if (Session != nullptr && Working != nullptr)
	{
		Session->ApplyAuthoredLevelSave(Working);
	}
	OnChanged.Broadcast(true);
}

bool UGatherAndHuntBoardEditor::Undo()
{
	if (UndoStack.Num() == 0)
	{
		return false;
	}
	RedoStack.Add(Working);
	Working = UndoStack.Pop();
	Cursor = ClampCell(Cursor, GetCellCount());
	CommitEdit();
	return true;
}

bool UGatherAndHuntBoardEditor::Redo()
{
	if (RedoStack.Num() == 0)
	{
		return false;
	}
	UndoStack.Add(Working);
	Working = RedoStack.Pop();
	Cursor = ClampCell(Cursor, GetCellCount());
	CommitEdit();
	return true;
}

void UGatherAndHuntBoardEditor::RemoveScriptedMovesFor(const int32 TurtleId)
{
	TArray<FScriptedOpponentTurnData>& Turns = Working->ScriptedOpponentTurns;
	for (FScriptedOpponentTurnData& Turn : Turns)
	{
		Turn.TurtlePlans.RemoveAll([TurtleId](const FScriptedPuzzleTurtlePlanData& Plan) { return Plan.TurtleId == TurtleId; });
	}
	Turns.RemoveAll([](const FScriptedOpponentTurnData& Turn) { return Turn.TurtlePlans.Num() == 0; });
}

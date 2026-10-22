#include "StructuralSupportComponent.h"
#include "StructuralRules.h"
#include "Buildables/BuildablePiece.h"

UStructuralSupportComponent::UStructuralSupportComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FIntVector UStructuralSupportComponent::GetPieceCell(const FVector& WorldLocation) const
{
	const float CellSize = Rules ? Rules->CellSize : 200.f;
	const float FloorHeight = Rules ? Rules->FloorHeight : 300.f;

	return FIntVector(
		FMath::RoundToInt(WorldLocation.X / CellSize),
		FMath::RoundToInt(WorldLocation.Y / CellSize),
		FMath::RoundToInt(WorldLocation.Z / FloorHeight));
}

int32 UStructuralSupportComponent::GetQuarterTurns(const FRotator& WorldRotation)
{
	return FMath::RoundToInt(WorldRotation.Yaw / 90.f);
}

FIntVector UStructuralSupportComponent::GetSocketCell(const ABuildablePiece* Piece, const FStructuralSocket& Socket) const
{
	if (!Piece)
	{
		return FIntVector::ZeroValue;
	}

	return Piece->GetStructuralCell() + RotateCellOffset(Socket.CellOffset, Piece->GetStructuralQuarterTurns());
}

int32 UStructuralSupportComponent::GetSocketRoofStep(const ABuildablePiece* Piece, const FStructuralSocket& Socket) const
{
	return Piece ? Piece->GetRoofStep() + Socket.RoofStep : 0;
}

FIntVector UStructuralSupportComponent::RotateCellOffset(const FIntVector& Offset, int32 QuarterTurns)
{
	FIntVector Result = Offset;
	const int32 Turns = ((QuarterTurns % 4) + 4) % 4;

	for (int32 i = 0; i < Turns; ++i)
	{
		Result = FIntVector(-Result.Y, Result.X, Result.Z);
	}

	return Result;
}

bool UStructuralSupportComponent::AreSocketsLinked(EStructuralSocketType A, EStructuralSocketType B, bool& OutIsVertical)
{
	const auto Matches = [A, B](EStructuralSocketType X, EStructuralSocketType Y)
	{
		return (A == X && B == Y) || (A == Y && B == X);
	};

	if (Matches(EStructuralSocketType::WallBottom, EStructuralSocketType::FloorEdge)
		|| Matches(EStructuralSocketType::WallBottom, EStructuralSocketType::FloorCorner)
		|| Matches(EStructuralSocketType::WallTop, EStructuralSocketType::FloorBottom)
		|| Matches(EStructuralSocketType::WallTop, EStructuralSocketType::RoofLow))
	{
		OutIsVertical = true;
		return true;
	}

	if (Matches(EStructuralSocketType::WallSide, EStructuralSocketType::WallSide)
		|| Matches(EStructuralSocketType::FloorEdge, EStructuralSocketType::FloorEdge)
		|| Matches(EStructuralSocketType::RoofLow, EStructuralSocketType::RoofLow)
		|| Matches(EStructuralSocketType::RoofHigh, EStructuralSocketType::RoofHigh)
		|| Matches(EStructuralSocketType::RoofSide, EStructuralSocketType::RoofSide)
		|| Matches(EStructuralSocketType::RoofHigh, EStructuralSocketType::RoofLow))
	{
		OutIsVertical = false;
		return true;
	}

	OutIsVertical = false;
	return false;
}

TMap<FIntVector, TObjectPtr<ABuildablePiece>>* UStructuralSupportComponent::GetOccupancyMap(EBuildAnchor SlotType)
{
	switch (SlotType)
	{
	case EBuildAnchor::Cell: return &FloorSlotOccupancy;
	case EBuildAnchor::Edge: return &EdgeSlotOccupancy;
	case EBuildAnchor::Vertex: return &VertexSlotOccupancy;
	default: return nullptr;
	}
}

const TMap<FIntVector, TObjectPtr<ABuildablePiece>>* UStructuralSupportComponent::GetOccupancyMap(EBuildAnchor SlotType) const
{
	return const_cast<UStructuralSupportComponent*>(this)->GetOccupancyMap(SlotType);
}

void UStructuralSupportComponent::OccupySlot(EBuildAnchor SlotType, const FIntVector& Cell, ABuildablePiece* Piece)
{
	if (TMap<FIntVector, TObjectPtr<ABuildablePiece>>* Map = GetOccupancyMap(SlotType))
	{
		Map->Add(Cell, Piece);
	}
}

void UStructuralSupportComponent::FreeSlotsFor(ABuildablePiece* Piece)
{
	if (!Piece)
	{
		return;
	}

	if (TMap<FIntVector, TObjectPtr<ABuildablePiece>>* Map = GetOccupancyMap(Piece->GetBuildAnchor()))
	{
		Map->Remove(Piece->GetGridCoord());
	}
}

bool UStructuralSupportComponent::IsSlotFree(EBuildAnchor SlotType, const FIntVector& Cell) const
{
	return GetPieceAt(SlotType, Cell) == nullptr;
}

ABuildablePiece* UStructuralSupportComponent::GetPieceAt(EBuildAnchor SlotType, const FIntVector& Cell) const
{
	const TMap<FIntVector, TObjectPtr<ABuildablePiece>>* Map = GetOccupancyMap(SlotType);

	if (!Map)
	{
		return nullptr;
	}

	const TObjectPtr<ABuildablePiece>* Found = Map->Find(Cell);
	return Found ? Found->Get() : nullptr;
}

void UStructuralSupportComponent::RegisterPiece(ABuildablePiece* Piece)
{
	if (!Piece || RegisteredPieces.Contains(Piece))
	{
		return;
	}

	RegisteredPieces.Add(Piece);
	Piece->SetStructuralComponent(this);

	if (Piece->GetBuildAnchor() != EBuildAnchor::Legacy)
	{
		OccupySlot(Piece->GetBuildAnchor(), Piece->GetGridCoord(), Piece);
	}

	bool bFoundAnchor = false;

	for (const FStructuralSocket& Socket : Piece->GetSockets())
	{
		if (Socket.Type == EStructuralSocketType::Anchor)
		{
			bFoundAnchor = true;
			continue;
		}

		const FIntVector AbsoluteCell = GetSocketCell(Piece, Socket);

		FRegisteredSocket NewEntry;
		NewEntry.Owner = Piece;
		NewEntry.Type = Socket.Type;
		NewEntry.RoofStep = GetSocketRoofStep(Piece, Socket);
		SocketsByCell.FindOrAdd(AbsoluteCell).Add(NewEntry);
	}

	Piece->SetIsStructuralAnchor(bFoundAnchor);
	RecomputeStability();
}

void UStructuralSupportComponent::UnregisterPiece(ABuildablePiece* Piece)
{
	if (!Piece || RegisteredPieces.Remove(Piece) == 0)
	{
		return;
	}

	FreeSlotsFor(Piece);

	for (auto It = SocketsByCell.CreateIterator(); It; ++It)
	{
		It.Value().RemoveAll([Piece](const FRegisteredSocket& Entry) { return Entry.Owner == Piece; });

		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}

	RecomputeStability();
}

float UStructuralSupportComponent::ComputeStability(const TArray<FStructuralSocket>& Sockets, const FVector& WorldLocation, const FRotator& WorldRotation, int32 RoofStep) const
{
	return ComputeStability(Sockets, GetPieceCell(WorldLocation), GetQuarterTurns(WorldRotation), RoofStep);
}

float UStructuralSupportComponent::ComputeStability(const TArray<FStructuralSocket>& Sockets, const FIntVector& Cell, int32 QuarterTurns, int32 RoofStep) const
{
	for (const FStructuralSocket& Socket : Sockets)
	{
		if (Socket.Type == EStructuralSocketType::Anchor)
		{
			return 1.f;
		}
	}

	const float VerticalLoss = Rules ? Rules->VerticalLoss : 0.1f;
	const float HorizontalLoss = Rules ? Rules->HorizontalLoss : 0.6f;
	float BestStability = 0.f;

	for (const FStructuralSocket& Socket : Sockets)
	{
		const FIntVector AbsoluteCell = Cell + RotateCellOffset(Socket.CellOffset, QuarterTurns);
		const int32 AbsoluteRoofStep = RoofStep + Socket.RoofStep;
		const TArray<FRegisteredSocket>* Entries = SocketsByCell.Find(AbsoluteCell);

		if (!Entries)
		{
			continue;
		}

		for (const FRegisteredSocket& Entry : *Entries)
		{
			bool bIsVertical = false;

			if (Entry.Owner && Entry.Owner->IsLoadBearing() && Entry.RoofStep == AbsoluteRoofStep && AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
			{
				const float Loss = bIsVertical ? VerticalLoss : HorizontalLoss;
				BestStability = FMath::Max(BestStability, Entry.Owner->GetStability() - Loss);
			}
		}
	}

	return BestStability;
}

void UStructuralSupportComponent::RecomputeStability()
{
	const float VerticalLoss = Rules ? Rules->VerticalLoss : 0.1f;
	const float HorizontalLoss = Rules ? Rules->HorizontalLoss : 0.6f;
	const float Threshold = Rules ? Rules->CollapseThreshold : 0.1f;

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (Piece)
		{
			Piece->SetStability(Piece->IsStructuralAnchor() ? 1.f : 0.f);
		}
	}

	// Fixed point relaxation: each pass can only raise a piece's stability, so this always
	// terminates - the safety cap just guards against an unforeseen cycle, it should never bite
	// for a house a few stories tall.
	bool bChanged = true;
	int32 SafetyIterations = 0;

	while (bChanged && SafetyIterations++ < 64)
	{
		bChanged = false;

		for (ABuildablePiece* Piece : RegisteredPieces)
		{
			if (!Piece || Piece->IsStructuralAnchor())
			{
				continue;
			}

			float BestStability = Piece->GetStability();

			for (const FStructuralSocket& Socket : Piece->GetSockets())
			{
				const FIntVector AbsoluteCell = GetSocketCell(Piece, Socket);
				const int32 AbsoluteRoofStep = GetSocketRoofStep(Piece, Socket);
				const TArray<FRegisteredSocket>* Entries = SocketsByCell.Find(AbsoluteCell);

				if (!Entries)
				{
					continue;
				}

				for (const FRegisteredSocket& Entry : *Entries)
				{
					if (!Entry.Owner || Entry.Owner == Piece || !Entry.Owner->IsLoadBearing() || Entry.RoofStep != AbsoluteRoofStep)
					{
						continue;
					}

					bool bIsVertical = false;

					if (AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
					{
						const float Loss = bIsVertical ? VerticalLoss : HorizontalLoss;
						BestStability = FMath::Max(BestStability, Entry.Owner->GetStability() - Loss);
					}
				}
			}

			if (BestStability > Piece->GetStability() + KINDA_SMALL_NUMBER)
			{
				Piece->SetStability(BestStability);
				bChanged = true;
			}
		}
	}

	// Collect first, then collapse: BeginCollapse unregisters (and recomputes) immediately, which
	// would otherwise mutate RegisteredPieces out from under this loop.
	TArray<ABuildablePiece*> ToCollapse;

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (Piece && !Piece->IsStructuralAnchor() && Piece->GetStability() < Threshold)
		{
			ToCollapse.Add(Piece);
		}
	}

	for (ABuildablePiece* Piece : ToCollapse)
	{
		Piece->BeginCollapse();
	}

	if (bShowDebug)
	{
		for (ABuildablePiece* Piece : RegisteredPieces)
		{
			if (Piece)
			{
				Piece->UpdateStabilityDebugColor();
			}
		}
	}
}

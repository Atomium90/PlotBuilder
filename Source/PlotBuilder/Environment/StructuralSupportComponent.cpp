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
		|| Matches(EStructuralSocketType::RoofSide, EStructuralSocketType::RoofSide))
	{
		OutIsVertical = false;
		return true;
	}

	OutIsVertical = false;
	return false;
}

void UStructuralSupportComponent::RegisterPiece(ABuildablePiece* Piece)
{
	if (!Piece || RegisteredPieces.Contains(Piece))
	{
		return;
	}

	RegisteredPieces.Add(Piece);
	Piece->SetStructuralComponent(this);

	const FIntVector PieceCell = GetPieceCell(Piece->GetActorLocation());
	const int32 QuarterTurns = GetQuarterTurns(Piece->GetActorRotation());
	bool bFoundAnchor = false;

	for (const FStructuralSocket& Socket : Piece->GetSockets())
	{
		if (Socket.Type == EStructuralSocketType::Anchor)
		{
			bFoundAnchor = true;
			continue;
		}

		const FIntVector AbsoluteCell = PieceCell + RotateCellOffset(Socket.CellOffset, QuarterTurns);

		FRegisteredSocket NewEntry;
		NewEntry.Owner = Piece;
		NewEntry.Type = Socket.Type;
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

bool UStructuralSupportComponent::WouldBeStable(const TArray<FStructuralSocket>& Sockets, const FVector& WorldLocation, const FRotator& WorldRotation) const
{
	for (const FStructuralSocket& Socket : Sockets)
	{
		if (Socket.Type == EStructuralSocketType::Anchor)
		{
			return true;
		}
	}

	const FIntVector PieceCell = GetPieceCell(WorldLocation);
	const int32 QuarterTurns = GetQuarterTurns(WorldRotation);
	const float VerticalLoss = Rules ? Rules->VerticalLoss : 0.1f;
	const float HorizontalLoss = Rules ? Rules->HorizontalLoss : 0.6f;
	float BestStability = 0.f;

	for (const FStructuralSocket& Socket : Sockets)
	{
		const FIntVector AbsoluteCell = PieceCell + RotateCellOffset(Socket.CellOffset, QuarterTurns);
		const TArray<FRegisteredSocket>* Entries = SocketsByCell.Find(AbsoluteCell);

		if (!Entries)
		{
			continue;
		}

		for (const FRegisteredSocket& Entry : *Entries)
		{
			bool bIsVertical = false;

			if (Entry.Owner && Entry.Owner->IsLoadBearing() && AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
			{
				const float Loss = bIsVertical ? VerticalLoss : HorizontalLoss;
				BestStability = FMath::Max(BestStability, Entry.Owner->GetStability() - Loss);
			}
		}
	}

	const float Threshold = Rules ? Rules->CollapseThreshold : 0.1f;
	return BestStability >= Threshold;
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

			const FIntVector PieceCell = GetPieceCell(Piece->GetActorLocation());
			const int32 QuarterTurns = GetQuarterTurns(Piece->GetActorRotation());
			float BestStability = Piece->GetStability();

			for (const FStructuralSocket& Socket : Piece->GetSockets())
			{
				const FIntVector AbsoluteCell = PieceCell + RotateCellOffset(Socket.CellOffset, QuarterTurns);
				const TArray<FRegisteredSocket>* Entries = SocketsByCell.Find(AbsoluteCell);

				if (!Entries)
				{
					continue;
				}

				for (const FRegisteredSocket& Entry : *Entries)
				{
					if (!Entry.Owner || Entry.Owner == Piece || !Entry.Owner->IsLoadBearing())
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

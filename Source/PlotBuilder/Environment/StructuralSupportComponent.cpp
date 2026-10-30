#include "StructuralSupportComponent.h"
#include "StructuralRules.h"
#include "Buildables/BuildablePiece.h"
#include "Destruction/DurabilityComponent.h"
#include "Components/StaticMeshComponent.h"

UStructuralSupportComponent::UStructuralSupportComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStructuralSupportComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRecomputeDirty)
	{
		bRecomputeDirty = false;
		RecomputeStability();
	}

	// Drains CollapseQueue every tick, independent of the dirty flag above - a cascade must keep
	// releasing pieces on ticks where nothing else changed.
	const float Now = GetWorld()->GetTimeSeconds();
	int32 DebugLine = 0;

	for (int32 Index = CollapseQueue.Num() - 1; Index >= 0; --Index)
	{
		ABuildablePiece* Piece = CollapseQueue[Index].Piece.Get();

		if (!Piece)
		{
			// Stale entry: the piece was destroyed by something else (e.g. a direct tool kill) while queued.
			CollapseQueue.RemoveAt(Index);
			continue;
		}

		if (Now >= CollapseQueue[Index].ReleaseTime)
		{
			Piece->BeginCollapse();
			CollapseQueue.RemoveAt(Index);
			continue;
		}

		if (bShowCollapseQueueDebug && GEngine && DebugLine < 20)
		{
			GEngine->AddOnScreenDebugMessage(800 + DebugLine++, DeltaTime, FColor::Yellow,
				FString::Printf(TEXT("%s releases in %.2fs"), *Piece->GetActorNameOrLabel(), CollapseQueue[Index].ReleaseTime - Now));
		}
	}
}

void UStructuralSupportComponent::SetShowDebug(bool bShow)
{
	bShowDebug = bShow;

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (!Piece || !Piece->GetMeshComponent())
		{
			continue;
		}

		Piece->GetMeshComponent()->SetMaterial(0, bShow ? DebugHeatmapMaterial : nullptr);

		if (bShow)
		{
			Piece->UpdateStabilityDebugColor();
		}
	}
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
	MarkDirty();
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
		const int32 RemovedCount = It.Value().RemoveAll([Piece](const FRegisteredSocket& Entry) { return Entry.Owner == Piece; });

		if (RemovedCount > 0)
		{
			// Seeds the collapse-queue BFS (see ComputeHopDistances) - a cell, not Piece itself, since
			// Piece may already be mid-Destroy() by the time RecomputeStability consumes this.
			RecentlyVacatedCells.Add(It.Key());
		}

		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}

	MarkDirty();
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
				const UDurabilityComponent* EntryDurability = Entry.Owner->GetDurabilityComponent();
				const float Coupling = EntryDurability ? EntryDurability->GetStabilityCoupling() : 1.f;
				BestStability = FMath::Max(BestStability, Entry.Owner->GetStability() * Coupling - Loss);
			}
		}
	}

	return BestStability;
}

TMap<ABuildablePiece*, int32> UStructuralSupportComponent::ComputeHopDistances() const
{
	TMap<ABuildablePiece*, int32> HopDistances;

	if (RecentlyVacatedCells.Num() == 0)
	{
		return HopDistances;
	}

	// Seed hop 0: any registered piece with a socket resolving to a vacated cell. The piece that
	// used to be there is already gone from SocketsByCell, so there's nothing left to match socket
	// types against - being anchored to the same cell it left is adjacency enough for cascade timing.
	TArray<ABuildablePiece*> Frontier;

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (!Piece || HopDistances.Contains(Piece))
		{
			continue;
		}

		for (const FStructuralSocket& Socket : Piece->GetSockets())
		{
			if (RecentlyVacatedCells.Contains(GetSocketCell(Piece, Socket)))
			{
				HopDistances.Add(Piece, 0);
				Frontier.Add(Piece);
				break;
			}
		}
	}

	// BFS outward over the same socket-adjacency graph the stability relaxation below walks, but
	// over ANY link (not just load-bearing) - hop distance is about physical graph distance for
	// cascade timing, not about who transmits stability to whom.
	for (int32 Index = 0; Index < Frontier.Num(); ++Index)
	{
		ABuildablePiece* Current = Frontier[Index];
		const int32 CurrentHop = HopDistances.FindRef(Current);

		for (const FStructuralSocket& Socket : Current->GetSockets())
		{
			const FIntVector AbsoluteCell = GetSocketCell(Current, Socket);
			const int32 AbsoluteRoofStep = GetSocketRoofStep(Current, Socket);
			const TArray<FRegisteredSocket>* Entries = SocketsByCell.Find(AbsoluteCell);

			if (!Entries)
			{
				continue;
			}

			for (const FRegisteredSocket& Entry : *Entries)
			{
				if (!Entry.Owner || Entry.Owner == Current || HopDistances.Contains(Entry.Owner) || Entry.RoofStep != AbsoluteRoofStep)
				{
					continue;
				}

				bool bIsVertical = false;

				if (AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
				{
					HopDistances.Add(Entry.Owner, CurrentHop + 1);
					Frontier.Add(Entry.Owner);
				}
			}
		}
	}

	return HopDistances;
}

void UStructuralSupportComponent::EnqueueCollapse(const TArray<ABuildablePiece*>& ToCollapse, const TMap<ABuildablePiece*, int32>& HopDistances)
{
	float BaseDelay = 0.25f;
	float HopDelay = 0.12f;
	float Jitter = 0.08f;
	float MaxCascadeSpread = 3.f;
	bool bHaveRules = false;

	struct FPendingEntry { ABuildablePiece* Piece; float SpreadDelay; };
	TArray<FPendingEntry> Pending;
	Pending.Reserve(ToCollapse.Num());

	for (ABuildablePiece* Piece : ToCollapse)
	{
		if (!Piece || CollapseQueue.ContainsByPredicate([Piece](const FQueuedCollapse& Entry) { return Entry.Piece.Get() == Piece; }))
		{
			continue;
		}

		if (!bHaveRules)
		{
			if (const UDurabilityComponent* Durability = Piece->GetDurabilityComponent())
			{
				BaseDelay = Durability->GetBaseDelay();
				HopDelay = Durability->GetHopDelay();
				Jitter = Durability->GetJitter();
				MaxCascadeSpread = Durability->GetMaxCascadeSpread();
				bHaveRules = true;
			}
		}

		const int32 Hop = HopDistances.FindRef(Piece);
		const float SpreadDelay = FMath::Max(0.f, Hop * HopDelay + FMath::RandRange(-Jitter, Jitter));
		Pending.Add({Piece, SpreadDelay});
	}

	if (Pending.Num() == 0)
	{
		return;
	}

	// Compress this batch's spread (hop*HopDelay + jitter only, BaseDelay is untouched) to fit
	// MaxCascadeSpread, preserving relative order - a big cascade stays within the cap without
	// flattening the difference between a hop-1 and a hop-5 piece.
	float MaxSpreadDelay = 0.f;

	for (const FPendingEntry& Entry : Pending)
	{
		MaxSpreadDelay = FMath::Max(MaxSpreadDelay, Entry.SpreadDelay);
	}

	const float CompressionScale = MaxSpreadDelay > MaxCascadeSpread ? MaxCascadeSpread / MaxSpreadDelay : 1.f;
	const float Now = GetWorld()->GetTimeSeconds();

	for (const FPendingEntry& Entry : Pending)
	{
		FQueuedCollapse Queued;
		Queued.Piece = Entry.Piece;
		Queued.ReleaseTime = Now + BaseDelay + Entry.SpreadDelay * CompressionScale;
		CollapseQueue.Add(Queued);
	}
}

void UStructuralSupportComponent::RecomputeStability()
{
	const TMap<ABuildablePiece*, int32> HopDistances = ComputeHopDistances();
	RecentlyVacatedCells.Reset();

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
						const UDurabilityComponent* EntryDurability = Entry.Owner->GetDurabilityComponent();
						const float Coupling = EntryDurability ? EntryDurability->GetStabilityCoupling() : 1.f;
						BestStability = FMath::Max(BestStability, Entry.Owner->GetStability() * Coupling - Loss);
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

	// Collect first, then enqueue: pieces no longer collapse immediately (see EnqueueCollapse) - they
	// join CollapseQueue and BeginCollapse only fires once their staggered release time comes up, in
	// TickComponent. Collecting into its own array still avoids mutating RegisteredPieces mid-loop.
	TArray<ABuildablePiece*> ToCollapse;

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (Piece && !Piece->IsStructuralAnchor() && Piece->GetStability() < Threshold)
		{
			ToCollapse.Add(Piece);
		}
	}

	EnqueueCollapse(ToCollapse, HopDistances);

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

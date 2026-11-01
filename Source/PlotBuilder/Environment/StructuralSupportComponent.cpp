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

	// Drains CompromisedQueue every tick, same reasoning as CollapseQueue above - GDD §7.6, D29.
	for (int32 Index = CompromisedQueue.Num() - 1; Index >= 0; --Index)
	{
		ABuildablePiece* Piece = CompromisedQueue[Index].Piece.Get();

		if (!Piece)
		{
			// Stale entry: the piece left some other way (e.g. a direct tool kill) while queued.
			CompromisedQueue.RemoveAt(Index);
			continue;
		}

		if (Now >= CompromisedQueue[Index].FailTime)
		{
			CompromisedQueue.RemoveAt(Index);

			// Stop the warning FX (and its jiggle timer) before BeginCollapse starts simulating
			// physics on the same mesh - OnCompromisedCancelled is also the right cleanup here even
			// though this piece didn't survive, it's the same "stop and restore" step either way.
			Piece->OnCompromisedCancelled();
			Piece->BeginCollapse();
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

FVector UStructuralSupportComponent::ComputeAwayFromNeighbourCentroid(const ABuildablePiece* Piece) const
{
	if (!Piece)
	{
		return FVector::ZeroVector;
	}

	TArray<const ABuildablePiece*> Neighbours;

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
			if (!Entry.Owner || Entry.Owner == Piece || Entry.RoofStep != AbsoluteRoofStep || Neighbours.Contains(Entry.Owner))
			{
				continue;
			}

			bool bIsVertical = false;

			if (AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
			{
				Neighbours.Add(Entry.Owner);
			}
		}
	}

	if (Neighbours.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	FVector Centroid = FVector::ZeroVector;

	for (const ABuildablePiece* Neighbour : Neighbours)
	{
		Centroid += Neighbour->GetActorLocation();
	}

	Centroid /= Neighbours.Num();

	FVector Direction = Piece->GetActorLocation() - Centroid;
	Direction.Z = 0.f;

	return Direction.IsNearlyZero() ? FVector::ZeroVector : Direction.GetSafeNormal();
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
		|| Matches(EStructuralSocketType::WallTop, EStructuralSocketType::RoofLow)
		// Stairwell wall link (GDD §8, D28): a wall stacked directly on another wall, with no floor
		// between them because a stairwell hole has none at any level by design. Without this, such
		// walls have no socket pair to pass stability through at all. Only passes a value along - a
		// wall stack still needs its own chain back down to an anchor (see T9).
		|| Matches(EStructuralSocketType::WallTop, EStructuralSocketType::WallBottom))
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

void UStructuralSupportComponent::RelaxStability(const ABuildablePiece* ExcludedPiece, TMap<const ABuildablePiece*, float>* DryRunResult) const
{
	const float VerticalLoss = Rules ? Rules->VerticalLoss : 0.1f;
	const float HorizontalLoss = Rules ? Rules->HorizontalLoss : 0.6f;

	// DryRunResult == nullptr: read/write each piece's own Stability field directly, exactly as
	// before this was factored out of RecomputeStability. DryRunResult != nullptr (WouldBeBottleneck):
	// read/write a scratch map instead, so a dry run never touches a real piece's Stability.
	auto ReadStability = [DryRunResult](const ABuildablePiece* Piece) -> float
	{
		if (DryRunResult)
		{
			const float* Found = DryRunResult->Find(Piece);
			return Found ? *Found : 0.f;
		}

		return Piece->GetStability();
	};

	auto WriteStability = [DryRunResult](ABuildablePiece* Piece, float Value)
	{
		if (DryRunResult)
		{
			DryRunResult->Add(Piece, Value);
		}
		else
		{
			Piece->SetStability(Value);
		}
	};

	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (Piece && Piece != ExcludedPiece)
		{
			WriteStability(Piece, Piece->IsStructuralAnchor() ? 1.f : 0.f);
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
			if (!Piece || Piece == ExcludedPiece || Piece->IsStructuralAnchor())
			{
				continue;
			}

			float BestStability = ReadStability(Piece);

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
					if (!Entry.Owner || Entry.Owner == Piece || Entry.Owner == ExcludedPiece
						|| !Entry.Owner->IsLoadBearing() || Entry.RoofStep != AbsoluteRoofStep)
					{
						continue;
					}

					bool bIsVertical = false;

					if (AreSocketsLinked(Socket.Type, Entry.Type, bIsVertical))
					{
						const float Loss = bIsVertical ? VerticalLoss : HorizontalLoss;
						const UDurabilityComponent* EntryDurability = Entry.Owner->GetDurabilityComponent();
						const float Coupling = EntryDurability ? EntryDurability->GetStabilityCoupling() : 1.f;
						BestStability = FMath::Max(BestStability, ReadStability(Entry.Owner) * Coupling - Loss);
					}
				}
			}

			// Sub-anchor slab (GDD §8, D28): resets to 1.0 like a real Anchor, but only once its own
			// incoming chain is itself valid - a floating sub-anchor must not hold up a building.
			// Re-checked every relaxation pass, so this naturally stops once the chain below it is
			// removed (RelaxStability re-seeds every non-anchor to 0 at the top, every time).
			if (Piece->ResetsStability() && BestStability >= (Rules ? Rules->SubAnchorMinimumIncomingStability : 0.1f))
			{
				BestStability = 1.f;
			}

			if (BestStability > ReadStability(Piece) + KINDA_SMALL_NUMBER)
			{
				WriteStability(Piece, BestStability);
				bChanged = true;
			}
		}
	}
}

bool UStructuralSupportComponent::WouldBeBottleneck(const ABuildablePiece* Candidate) const
{
	if (!Candidate)
	{
		return false;
	}

	TMap<const ABuildablePiece*, float> DryRun;
	RelaxStability(Candidate, &DryRun);

	const float Threshold = Rules ? Rules->CollapseThreshold : 0.1f;

	for (const TPair<const ABuildablePiece*, float>& Pair : DryRun)
	{
		// Only counts if removing Candidate is what pushes this piece under the threshold - a piece
		// already below it for an unrelated reason (e.g. mid-batch, already queued to collapse on
		// its own) would show up here regardless of Candidate, and must not make Candidate a
		// bottleneck by coincidence.
		if (Pair.Key != Candidate && Pair.Value < Threshold && Pair.Key->GetStability() >= Threshold)
		{
			return true;
		}
	}

	return false;
}

void UStructuralSupportComponent::NotifyEnteredCritical(ABuildablePiece* Piece)
{
	if (Piece && WouldBeBottleneck(Piece))
	{
		StartCompromised(Piece);
	}
}

void UStructuralSupportComponent::StartCompromised(ABuildablePiece* Piece)
{
	if (!Piece || CompromisedQueue.ContainsByPredicate([Piece](const FCompromisedEntry& Entry) { return Entry.Piece.Get() == Piece; }))
	{
		return;
	}

	const UDurabilityComponent* Durability = Piece->GetDurabilityComponent();
	const float DelayMin = Durability ? Durability->GetCompromisedDelayMin() : 0.5f;
	const float DelayMax = Durability ? Durability->GetCompromisedDelayMax() : 3.f;

	FCompromisedEntry Entry;
	Entry.Piece = Piece;
	Entry.FailTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(DelayMin, DelayMax);
	CompromisedQueue.Add(Entry);

	Piece->OnCompromisedStart();
}

void UStructuralSupportComponent::CancelCompromised(ABuildablePiece* Piece)
{
	const int32 RemovedCount = CompromisedQueue.RemoveAll([Piece](const FCompromisedEntry& Entry) { return Entry.Piece.Get() == Piece; });

	if (RemovedCount > 0 && Piece)
	{
		Piece->OnCompromisedCancelled();
	}
}

void UStructuralSupportComponent::RefreshCompromisedQueue()
{
	// GDD §7.6, D29's "again after each batch of destructions" - re-checks every currently Critical,
	// load-bearing piece: starts (or leaves running) a fail timer for a genuine bottleneck, cancels
	// one for a piece that no longer is (its dependants were destroyed first) or isn't Critical any
	// more. Called once per RecomputeStability, same cadence as the collapse queue above it.
	for (ABuildablePiece* Piece : RegisteredPieces)
	{
		if (!Piece || !Piece->IsLoadBearing())
		{
			continue;
		}

		const UDurabilityComponent* Durability = Piece->GetDurabilityComponent();

		if (!Durability || Durability->GetState() != EDurabilityState::Critical)
		{
			CancelCompromised(Piece);
			continue;
		}

		if (WouldBeBottleneck(Piece))
		{
			StartCompromised(Piece);
		}
		else
		{
			CancelCompromised(Piece);
		}
	}
}

void UStructuralSupportComponent::RecomputeStability()
{
	const TMap<ABuildablePiece*, int32> HopDistances = ComputeHopDistances();
	RecentlyVacatedCells.Reset();

	const float Threshold = Rules ? Rules->CollapseThreshold : 0.1f;

	RelaxStability(nullptr, nullptr);

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
	RefreshCompromisedQueue();

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

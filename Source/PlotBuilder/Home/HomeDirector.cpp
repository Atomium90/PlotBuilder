#include "HomeDirector.h"
#include "Debug/DebugDisplay.h"
#include "Buildables/BuildablePiece.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Environment/Plot.h"
#include "Environment/StructuralSupportComponent.h"
#include "Interaction/HotbarComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/PlayerCharacter.h"
#include "PlotBuilder.h"
#include "Progress/ProgressSave.h"
#include "Progress/ProgressSubsystem.h"
#include "Props/PhysicsProp.h"
#include "TimerManager.h"

AHomeDirector::AHomeDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

AHomeDirector* AHomeDirector::Find(const UObject* WorldContext)
{
	return Cast<AHomeDirector>(UGameplayStatics::GetActorOfClass(WorldContext, AHomeDirector::StaticClass()));
}

APlot* AHomeDirector::ResolvePlot() const
{
	if (Plot)
	{
		return Plot;
	}

	return Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass()));
}

void AHomeDirector::BeginPlay()
{
	Super::BeginPlay();

	Plot = ResolvePlot();

	// Whatever props the level holds right now were put there by hand: they are part of the level, not of the house.
	for (TActorIterator<APhysicsProp> It(GetWorld()); It; ++It)
	{
		LevelProps.Add(*It);
	}

	if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
	{
		LeaveHandle = Progress->OnBeforeLevelChange.AddUObject(this, &AHomeDirector::SaveSnapshot);
	}

	if (bRestoreOnBeginPlay)
	{
		RestorePlot();
		ApplyInventory();
	}
}

void AHomeDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LeaveHandle.IsValid())
	{
		if (UProgressSubsystem* Progress = UProgressSubsystem::Get(this))
		{
			Progress->OnBeforeLevelChange.Remove(LeaveHandle);
		}

		LeaveHandle.Reset();
	}

	GetWorldTimerManager().ClearTimer(InventoryTimer);

	Super::EndPlay(EndPlayReason);
}

void AHomeDirector::RestorePlot()
{
	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);

	if (!Plot || !Progress)
	{
		Summary = TEXT("Home: no plot or no progress subsystem, nothing restored.");
		Report(FColor::Red);
		return;
	}

	const FHomeSnapshot& Home = Progress->GetHome();

	if (!Home.bValid)
	{
		Summary = TEXT("Home: nothing saved yet, the plot starts empty.");
		Report(FColor::Cyan);
		return;
	}

	// Lowest storey first, so the log and the first frame read bottom to top. The graph does not need the order:
	// registering only marks it dirty, the recompute runs once on the next tick over the whole house.
	TArray<FHomePieceRecord> Pieces = Home.Pieces;
	Pieces.Sort([](const FHomePieceRecord& A, const FHomePieceRecord& B)
	{
		if (A.Cell.Z != B.Cell.Z)
		{
			return A.Cell.Z < B.Cell.Z;
		}

		return A.Cell.X != B.Cell.X ? A.Cell.X < B.Cell.X : A.Cell.Y < B.Cell.Y;
	});

	int32 PiecesPlaced = 0;
	int32 PiecesRefused = 0;

	for (const FHomePieceRecord& Record : Pieces)
	{
		UClass* PieceClass = Record.PieceClass.LoadSynchronous();
		const ABuildablePiece* Piece = PieceClass
			? Plot->PlacePiece(TSubclassOf<ABuildablePiece>(PieceClass), Record.Cell, Record.QuarterTurns, Record.RoofStep, Record.Scale)
			: nullptr;

		if (Piece)
		{
			++PiecesPlaced;
		}
		else
		{
			++PiecesRefused;
		}
	}

	int32 PropsPlaced = 0;
	const FTransform PlotTransform = Plot->GetActorTransform();

	for (const FHomePropRecord& Record : Home.Props)
	{
		UClass* PropClass = Record.ActorClass.LoadSynchronous();

		if (!PropClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if (GetWorld()->SpawnActor<AActor>(PropClass, Record.PlotRelativeTransform * PlotTransform, SpawnParams))
		{
			++PropsPlaced;
		}
	}

	Summary = FString::Printf(TEXT("Home restored: %d pieces (%d refused), %d of %d props"), PiecesPlaced, PiecesRefused, PropsPlaced, Home.Props.Num());
	Report(PiecesRefused > 0 ? FColor::Orange : FColor::Green);
}

void AHomeDirector::ApplyInventory()
{
	const UProgressSubsystem* Progress = UProgressSubsystem::Get(this);

	if (!Progress || !Progress->GetHome().bValid)
	{
		GetWorldTimerManager().ClearTimer(InventoryTimer);
		return;
	}

	const APlayerCharacter* Character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	UHotbarComponent* Hotbar = Character ? Character->GetHotbarComponent() : nullptr;

	if (Hotbar)
	{
		const TArray<FHomeSlotRecord>& Records = Progress->GetHome().Slots;

		TArray<FInventorySlot> Slots;
		Slots.SetNum(Records.Num());

		int32 Filled = 0;

		for (int32 Index = 0; Index < Records.Num(); ++Index)
		{
			Slots[Index].ActorClass = Records[Index].ActorClass.LoadSynchronous();
			Slots[Index].Scale = Records[Index].Scale;
			Slots[Index].Count = Records[Index].Count;
			Slots[Index].StackSize = Records[Index].StackSize;

			if (!Slots[Index].IsEmpty())
			{
				++Filled;
			}
		}

		Hotbar->RestoreSlots(Slots);

		Summary += FString::Printf(TEXT(", inventory %d stacks"), Filled);
		Report(FColor::Green);

		GetWorldTimerManager().ClearTimer(InventoryTimer);
		return;
	}

	if (++InventoryAttempts > MaxInventoryAttempts)
	{
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHomeDirector: no player character after %d tries, the inventory is not restored."), MaxInventoryAttempts);
		GetWorldTimerManager().ClearTimer(InventoryTimer);
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(InventoryTimer))
	{
		GetWorldTimerManager().SetTimer(InventoryTimer, this, &AHomeDirector::ApplyInventory, InventoryRetryInterval, true);
	}
}

void AHomeDirector::SaveSnapshot()
{
	UProgressSubsystem* Progress = UProgressSubsystem::Get(this);
	const UStructuralSupportComponent* Support = Plot ? Plot->GetStructuralSupport() : nullptr;

	if (!Progress || !Support)
	{
		// Never write an empty snapshot over a good one because a reference was missing.
		UE_LOG(LogPlotBuilder, Warning, TEXT("AHomeDirector: no plot or no progress subsystem, the home is not saved."));
		return;
	}

	FHomeSnapshot Snapshot;
	Snapshot.bValid = true;

	int32 LegacySkipped = 0;

	for (const TObjectPtr<ABuildablePiece>& Entry : Support->GetRegisteredPieces())
	{
		const ABuildablePiece* Piece = Entry;

		if (!IsValid(Piece) || Piece->State != EBuildablePieceState::Placed)
		{
			continue;
		}

		// A Legacy piece has no cell: the shared placement function refuses it, so there is nothing to restore it with.
		if (Piece->GetBuildAnchor() == EBuildAnchor::Legacy)
		{
			++LegacySkipped;
			continue;
		}

		FHomePieceRecord& Record = Snapshot.Pieces.AddDefaulted_GetRef();
		Record.PieceClass = TSoftClassPtr<ABuildablePiece>(Piece->GetClass());
		Record.Cell = Piece->GetGridCoord();
		Record.QuarterTurns = Piece->GetOrientation();
		Record.RoofStep = Piece->GetRoofStep();
		Record.Scale = Piece->GetPlacedScale();
	}

	const FTransform PlotTransform = Plot->GetActorTransform();

	for (TActorIterator<APhysicsProp> It(GetWorld()); It; ++It)
	{
		APhysicsProp* Prop = *It;

		if (!IsValid(Prop) || Prop->IsActorBeingDestroyed() || LevelProps.Contains(Prop))
		{
			continue;
		}

		FHomePropRecord& Record = Snapshot.Props.AddDefaulted_GetRef();
		Record.ActorClass = TSoftClassPtr<AActor>(Prop->GetClass());
		Record.PlotRelativeTransform = Prop->GetActorTransform().GetRelativeTransform(PlotTransform);
	}

	if (const APlayerCharacter* Character = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (const UHotbarComponent* Hotbar = Character->GetHotbarComponent())
		{
			for (const FInventorySlot& Slot : Hotbar->GetSlots())
			{
				FHomeSlotRecord& Record = Snapshot.Slots.AddDefaulted_GetRef();
				Record.ActorClass = TSoftClassPtr<AActor>(Slot.ActorClass.Get());
				Record.Scale = Slot.Scale;
				Record.Count = Slot.Count;
				Record.StackSize = Slot.StackSize;
			}
		}
	}

	Snapshot.Pieces.Sort([](const FHomePieceRecord& A, const FHomePieceRecord& B)
	{
		if (A.Cell.Z != B.Cell.Z)
		{
			return A.Cell.Z < B.Cell.Z;
		}

		return A.Cell.X != B.Cell.X ? A.Cell.X < B.Cell.X : A.Cell.Y < B.Cell.Y;
	});

	Summary = FString::Printf(TEXT("Home saved: %d pieces (%d legacy skipped), %d props, %d slots"),
		Snapshot.Pieces.Num(), LegacySkipped, Snapshot.Props.Num(), Snapshot.Slots.Num());

	Progress->SetHome(MoveTemp(Snapshot));
	Report(LegacySkipped > 0 ? FColor::Orange : FColor::Green);
}

void AHomeDirector::Report(const FColor& Color) const
{
	UE_LOG(LogPlotBuilder, Log, TEXT("AHomeDirector: %s"), *Summary);

	if (GEngine)
	{
		PBDebug::Message(SummaryMessageKey, 8.f, Color, Summary);
	}
}

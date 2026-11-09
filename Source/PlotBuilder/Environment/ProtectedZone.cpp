#include "Environment/ProtectedZone.h"
#include "Buildables/BuildablePiece.h"
#include "Components/BoxComponent.h"
#include "Contracts/MissionBinding.h"
#include "Debug/DebugDisplay.h"
#include "Destruction/Damageable.h"
#include "Destruction/DurabilityComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Environment/Plot.h"
#include "Kismet/GameplayStatics.h"
#include "PlotBuilder.h"

AProtectedZone::AProtectedZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	SetRootComponent(Volume);

	Volume->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	Volume->SetCollisionProfileName(FName("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);
}

void AProtectedZone::BeginPlay()
{
	Super::BeginPlay();

	CachedPlot = Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass()));

	// Bound whether the zone is on or not: it may be switched on later, and an off zone has no overlap events anyway.
	Volume->OnComponentBeginOverlap.AddDynamic(this, &AProtectedZone::OnVolumeBeginOverlap);

	// The mission director may have switched this zone off before it started (it runs first): it stays off, out of the plot's list.
	if (!bActive)
	{
		SetActorTickEnabled(false);
		return;
	}

	if (APlot* Plot = CachedPlot.Get())
	{
		Plot->RegisterProtectedZone(this);
	}

	// What already stands inside when the level starts: its overlaps began before this delegate was bound.
	TArray<AActor*> Inside;
	Volume->GetOverlappingActors(Inside);

	for (AActor* Actor : Inside)
	{
		Track(Actor);
	}

	// The overlaps above depend on when the physics scene computed them; the scan below does not, it reads the bounds of what stands in the volume.
	ScanVolume();

	SetActorTickEnabled(bShowDebug);
}

void AProtectedZone::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bActive)
	{
		Track(OtherActor);
	}
}

void AProtectedZone::SetActive(bool bNewActive)
{
	if (bActive == bNewActive)
	{
		return;
	}

	bActive = bNewActive;

	// The profile's own collision is query only (OverlapAllDynamic): back to that when on.
	Volume->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Volume->SetGenerateOverlapEvents(bActive);

	APlot* Plot = CachedPlot.Get();

	if (bActive)
	{
		ShowContent();
		SetActorTickEnabled(bShowDebug);

		// Before BeginPlay it is BeginPlay that registers and scans.
		if (HasActorBegunPlay())
		{
			if (Plot)
			{
				Plot->RegisterProtectedZone(this);
			}

			ScanVolume();
		}
	}
	else
	{
		SetActorTickEnabled(false);

		if (Plot)
		{
			Plot->UnregisterProtectedZone(this);
		}

		ClearWatching();
		HideContent();
	}
}

void AProtectedZone::ClearWatching()
{
	for (FProtectedRecord& Record : Records)
	{
		const AActor* Actor = Record.Actor.Get();

		if (UDurabilityComponent* Durability = IsValid(Actor) ? Actor->FindComponentByClass<UDurabilityComponent>() : nullptr)
		{
			Durability->OnDurabilityDamaged.RemoveDynamic(this, &AProtectedZone::HandleDamaged);
		}
	}

	Records.Reset();
	DamageByTool.Reset();
	ExternalDamage = 0.f;
	ExternalBySource.Reset();
	Fallen.Reset();
	CountedPieces.Reset();
}

void AProtectedZone::CollectProtectedThings(TArray<AActor*>& OutThings) const
{
	const UWorld* World = GetWorld();

	if (!World || !Volume)
	{
		return;
	}

	const FBox ZoneBox = Volume->Bounds.GetBox();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		// The house, the plot and the player are not the zone's to hide; only what could be damaged stands for a protected thing.
		if (Actor == this || Actor->IsA<ABuildablePiece>() || Actor->IsA<APlot>() || Actor->IsA<APawn>() || !Actor->Implements<UDamageable>())
		{
			continue;
		}

		const FBox Box = Actor->GetComponentsBoundingBox(true);

		if (Box.IsValid && ZoneBox.Intersect(Box))
		{
			OutThings.Add(Actor);
		}
	}
}

void AProtectedZone::HideContent()
{
	if (!bHideContentWhenInactive)
	{
		return;
	}

	TArray<AActor*> Things;
	CollectProtectedThings(Things);

	for (AActor* Thing : Things)
	{
		Thing->SetActorHiddenInGame(true);
		Thing->SetActorEnableCollision(false);
		HiddenContent.AddUnique(Thing);
	}
}

void AProtectedZone::ShowContent()
{
	for (const TWeakObjectPtr<AActor>& Hidden : HiddenContent)
	{
		if (AActor* Thing = Hidden.Get())
		{
			Thing->SetActorHiddenInGame(false);
			Thing->SetActorEnableCollision(true);
		}
	}

	HiddenContent.Reset();
}

void AProtectedZone::ScanVolume()
{
	const UWorld* World = GetWorld();

	if (!World || !Volume)
	{
		return;
	}

	const FBox ZoneBox = Volume->Bounds.GetBox();
	int32 InVolume = 0;
	FString Lines;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;

		// The house is not what this zone protects, and nothing else is interesting to list but the other actors that share the volume.
		if (Actor == this || Actor->IsA<ABuildablePiece>() || Actor->IsA<APlot>())
		{
			continue;
		}

		const FBox Box = Actor->GetComponentsBoundingBox(true);

		if (!Box.IsValid || !ZoneBox.Intersect(Box))
		{
			continue;
		}

		++InVolume;

		FString Why;
		const bool bWatched = Track(Actor, &Why);

		if (InVolume <= 12)
		{
			Lines += FString::Printf(TEXT("\n  %s: %s"), *Actor->GetName(), bWatched ? TEXT("now watched") : *Why);
		}
	}

	UE_LOG(LogPlotBuilder, Log, TEXT("AProtectedZone '%s': scanned its volume (centre %s, half size %s): %d actor(s) share it, %d thing(s) watched in all.%s"),
		*GetName(), *Volume->GetComponentLocation().ToCompactString(), *Volume->GetScaledBoxExtent().ToCompactString(), InVolume, Records.Num(), *Lines);
}

bool AProtectedZone::Track(AActor* Actor, FString* OutWhyNot)
{
	const auto Refuse = [OutWhyNot](const TCHAR* Why)
	{
		if (OutWhyNot)
		{
			*OutWhyNot = Why;
		}

		return false;
	};

	if (!Actor || !Actor->Implements<UDamageable>())
	{
		return Refuse(TEXT("not watched, it cannot be damaged (no IDamageable)"));
	}

	UDurabilityComponent* Durability = Actor->FindComponentByClass<UDurabilityComponent>();

	if (!Durability)
	{
		return Refuse(TEXT("not watched, it has no durability component"));
	}

	for (const FProtectedRecord& Existing : Records)
	{
		if (Existing.Actor == Actor)
		{
			return Refuse(TEXT("already watched"));
		}
	}

	// The house being demolished may overlap the volume (a piece falling in, a plot next to it): it is the job, not collateral.
	// Any piece counts as the house, in a set or not: the leaf of a door or the glass of a window is a piece of its own that no set holds.
	if (Actor->IsA<ABuildablePiece>())
	{
		return Refuse(TEXT("not watched, it is a piece of the house"));
	}

	if (Actor->IsA<APawn>())
	{
		return Refuse(TEXT("not watched, it is a pawn (the player walking through is not a protected thing)"));
	}

	if (const APlot* Plot = CachedPlot.Get())
	{
		if (Plot->GetMissionSets().IsTarget(Actor) || Plot->GetMissionSets().IsMustStay(Actor) || Plot->GetMissionSets().IsSalvage(Actor))
		{
			return Refuse(TEXT("not watched, it is an item of the mission's house"));
		}
	}

	FProtectedRecord& Record = Records.AddDefaulted_GetRef();
	Record.Actor = Actor;
	Record.Name = Actor->GetName();
	Record.StartHP = Durability->CurrentHP;
	Record.HPLost = 0.f;
	Record.bDestroyed = Durability->GetState() == EDurabilityState::Destroyed;
	Record.bStartPending = Durability->MaxHP <= 0.f;

	Durability->OnDurabilityDamaged.AddDynamic(this, &AProtectedZone::HandleDamaged);

	UE_LOG(LogPlotBuilder, Log, TEXT("AProtectedZone '%s': now watching '%s' (%s)"), *GetName(), *Record.Name,
		Record.bStartPending ? TEXT("hit points not set yet, taken at the next look") : *FString::Printf(TEXT("%.0f HP"), Record.StartHP));
	return true;
}

void AProtectedZone::HandleDamaged(const FDamageInfo& DamageInfo)
{
	DamageByTool.FindOrAdd(DamageInfo.ToolId) += DamageInfo.Amount;
	SyncRecords();
}

void AProtectedZone::SyncRecords() const
{
	for (FProtectedRecord& Record : Records)
	{
		const AActor* Actor = Record.Actor.Get();
		const UDurabilityComponent* Durability = IsValid(Actor) ? Actor->FindComponentByClass<UDurabilityComponent>() : nullptr;

		if (Durability)
		{
			// It set its hit points since the zone started watching it: it starts from full health, whatever it lost in between
			// (nothing can have been dealt to it before its own BeginPlay).
			if (Record.bStartPending && Durability->MaxHP > 0.f)
			{
				Record.StartHP = Durability->MaxHP;
				Record.bStartPending = false;
			}

			Record.HPLost = FMath::Clamp(Record.StartHP - Durability->CurrentHP, 0.f, Record.StartHP);
			Record.bDestroyed = Durability->GetState() == EDurabilityState::Destroyed;
		}
		else
		{
			// Gone: a thing that was removed lost everything it had.
			Record.HPLost = Record.StartHP;
			Record.bDestroyed = true;
		}
	}
}

const TArray<FProtectedRecord>& AProtectedZone::GetRecords() const
{
	SyncRecords();
	return Records;
}

float AProtectedZone::GetTotalStartHP() const
{
	float Total = 0.f;

	for (const FProtectedRecord& Record : GetRecords())
	{
		Total += Record.StartHP;
	}

	return Total;
}

float AProtectedZone::GetTotalHPLost() const
{
	float Total = 0.f;

	for (const FProtectedRecord& Record : GetRecords())
	{
		Total += Record.HPLost;
	}

	return Total;
}

float AProtectedZone::GetDamageRatio() const
{
	const float StartHP = GetTotalStartHP();
	return StartHP > 0.f ? GetTotalHPLost() / StartHP : 0.f;
}

void AProtectedZone::RecordExternalDamage(FName Source, float Amount)
{
	if (Amount <= 0.f)
	{
		return;
	}

	ExternalDamage += Amount;
	ExternalBySource.FindOrAdd(Source) += Amount;
}

void AProtectedZone::NotifyPieceLanded(AActor* Piece, int32 Weight)
{
	if (!IsValid(Piece) || Weight <= 0 || CountedPieces.Contains(Piece))
	{
		return;
	}

	const FBox PieceBox = Piece->GetComponentsBoundingBox(true);

	if (!PieceBox.IsValid)
	{
		return;
	}

	const FBox ZoneBox = Volume->Bounds.GetBox();
	const bool bInside = bCountTouching ? ZoneBox.Intersect(PieceBox) : ZoneBox.IsInside(PieceBox.GetCenter());

	if (!bInside)
	{
		// Verbose, like the line in APlot::NotifyPieceLanded: most pieces of a collapse land outside the zone.
		UE_LOG(LogPlotBuilder, Verbose, TEXT("AProtectedZone '%s': %s broke outside (its box centre %s, the zone's box %s to %s)"),
			*GetName(), *Piece->GetName(), *PieceBox.GetCenter().ToCompactString(), *ZoneBox.Min.ToCompactString(), *ZoneBox.Max.ToCompactString());
		return;
	}

	CountedPieces.Add(Piece);

	FFallenRecord& Record = Fallen.AddDefaulted_GetRef();
	Record.Name = MissionBinding::ItemName(Piece);
	Record.Weight = Weight;

	RecordExternalDamage(FName(*Record.Name), static_cast<float>(Weight));

	UE_LOG(LogPlotBuilder, Log, TEXT("AProtectedZone '%s': a %s of the house fell in (weight %d), %.0f fallen in so far"), *GetName(), *Record.Name, Weight, ExternalDamage);

	if (PBDebug::IsEnabled())
	{
		// A box where it landed, for a few seconds: the piece and its fragments are gone by then, this is what shows what counted.
		DrawDebugBox(GetWorld(), PieceBox.GetCenter(), PieceBox.GetExtent(), FColor::Orange, false, 8.f, 0, 3.f);

		if (GEngine)
		{
			PBDebug::Message(-1, 4.f, FColor::Orange, FString::Printf(TEXT("Protected zone '%s': a %s fell in (weight %d)"), *GetName(), *Record.Name, Weight));
		}
	}
}

void AProtectedZone::ResetRecords()
{
	DamageByTool.Reset();
	ExternalDamage = 0.f;
	ExternalBySource.Reset();
	Fallen.Reset();
	CountedPieces.Reset();

	// From what each thing has left now, so only new damage counts. A thing already gone has nothing left to lose: dropped.
	for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
	{
		FProtectedRecord& Record = Records[Index];
		const AActor* Actor = Record.Actor.Get();
		const UDurabilityComponent* Durability = IsValid(Actor) ? Actor->FindComponentByClass<UDurabilityComponent>() : nullptr;

		if (!Durability)
		{
			Records.RemoveAt(Index);
			continue;
		}

		Record.StartHP = Durability->CurrentHP;
		Record.HPLost = 0.f;
		Record.bDestroyed = Durability->GetState() == EDurabilityState::Destroyed;
		Record.bStartPending = Durability->MaxHP <= 0.f;
	}
}

FString AProtectedZone::Describe() const
{
	if (!bActive)
	{
		return FString::Printf(TEXT("Protected zone '%s': off for this mission"), *GetName());
	}

	FString Text = FString::Printf(TEXT("Protected zone '%s': %.0f of %.0f HP lost (%.0f%%)"), *GetName(), GetTotalHPLost(), GetTotalStartHP(), GetDamageRatio() * 100.f);

	for (const FProtectedRecord& Record : GetRecords())
	{
		Text += FString::Printf(TEXT("\n  %s: %.0f of %.0f HP lost%s"), *Record.Name, Record.HPLost, Record.StartHP, Record.bDestroyed ? TEXT(" (destroyed)") : TEXT(""));
	}

	for (const TPair<FName, float>& Pair : DamageByTool)
	{
		Text += FString::Printf(TEXT("\n  from %s: %.0f"), *Pair.Key.ToString(), Pair.Value);
	}

	// What fell in from the house, piece by piece, then any other source of external damage that is not a piece.
	for (const FFallenRecord& Record : Fallen)
	{
		Text += FString::Printf(TEXT("\n  fell in: %s (weight %d)"), *Record.Name, Record.Weight);
	}

	if (ExternalDamage > 0.f)
	{
		Text += FString::Printf(TEXT("\n  weight fallen in or reported from outside: %.0f"), ExternalDamage);
	}

	return Text;
}

void AProtectedZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebug && PBDebug::IsEnabled())
	{
		const FColor Color = (GetTotalHPLost() > 0.f || ExternalDamage > 0.f) ? FColor::Red : FColor::Yellow;
		DrawDebugBox(GetWorld(), Volume->GetComponentLocation(), Volume->GetScaledBoxExtent(), Volume->GetComponentQuat(), Color, false, -1.f, 0, 3.f);
	}
}

#include "Environment/ProtectedZone.h"
#include "Components/BoxComponent.h"
#include "Debug/DebugDisplay.h"
#include "Destruction/Damageable.h"
#include "Destruction/DurabilityComponent.h"
#include "DrawDebugHelpers.h"
#include "Environment/Plot.h"
#include "Kismet/GameplayStatics.h"

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

	if (APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass())))
	{
		Plot->RegisterProtectedZone(this);
	}

	Volume->OnComponentBeginOverlap.AddDynamic(this, &AProtectedZone::OnVolumeBeginOverlap);

	// What already stands inside when the level starts: its overlaps began before this delegate was bound.
	TArray<AActor*> Inside;
	Volume->GetOverlappingActors(Inside);

	for (AActor* Actor : Inside)
	{
		Track(Actor);
	}

	SetActorTickEnabled(bShowDebug);
}

void AProtectedZone::OnVolumeBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	Track(OtherActor);
}

void AProtectedZone::Track(AActor* Actor)
{
	if (!Actor || !Actor->Implements<UDamageable>())
	{
		return;
	}

	UDurabilityComponent* Durability = Actor->FindComponentByClass<UDurabilityComponent>();

	if (!Durability)
	{
		return;
	}

	for (const FProtectedRecord& Record : Records)
	{
		if (Record.Actor == Actor)
		{
			return;
		}
	}

	// The house being demolished may overlap the volume (a piece falling in, a plot next to it): it is the job, not collateral.
	if (const APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass())))
	{
		if (Plot->GetMissionSets().IsTarget(Actor) || Plot->GetMissionSets().IsMustStay(Actor))
		{
			return;
		}
	}

	FProtectedRecord& Record = Records.AddDefaulted_GetRef();
	Record.Actor = Actor;
	Record.Name = Actor->GetName();
	Record.StartHP = Durability->CurrentHP;
	Record.HPLost = 0.f;
	Record.bDestroyed = Durability->GetState() == EDurabilityState::Destroyed;

	Durability->OnDurabilityDamaged.AddDynamic(this, &AProtectedZone::HandleDamaged);
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
	DamageByTool.FindOrAdd(Source) += Amount;
}

void AProtectedZone::ResetRecords()
{
	DamageByTool.Reset();
	ExternalDamage = 0.f;

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
	}
}

FString AProtectedZone::Describe() const
{
	FString Text = FString::Printf(TEXT("Protected zone '%s': %.0f of %.0f HP lost (%.0f%%)"), *GetName(), GetTotalHPLost(), GetTotalStartHP(), GetDamageRatio() * 100.f);

	for (const FProtectedRecord& Record : GetRecords())
	{
		Text += FString::Printf(TEXT("\n  %s: %.0f of %.0f HP lost%s"), *Record.Name, Record.HPLost, Record.StartHP, Record.bDestroyed ? TEXT(" (destroyed)") : TEXT(""));
	}

	for (const TPair<FName, float>& Pair : DamageByTool)
	{
		Text += FString::Printf(TEXT("\n  from %s: %.0f"), *Pair.Key.ToString(), Pair.Value);
	}

	if (ExternalDamage > 0.f)
	{
		Text += FString::Printf(TEXT("\n  external: %.0f"), ExternalDamage);
	}

	return Text;
}

void AProtectedZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebug && PBDebug::IsEnabled())
	{
		const FColor Color = GetTotalHPLost() > 0.f ? FColor::Red : FColor::Yellow;
		DrawDebugBox(GetWorld(), Volume->GetComponentLocation(), Volume->GetScaledBoxExtent(), Volume->GetComponentQuat(), Color, false, -1.f, 0, 3.f);
	}
}

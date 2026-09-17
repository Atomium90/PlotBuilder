#include "BuildModeComponent.h"
#include "HotbarComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

UBuildModeComponent::UBuildModeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UBuildModeComponent::BeginPlay()
{
	Super::BeginPlay();

	HotbarComponent = GetOwner() ? GetOwner()->FindComponentByClass<UHotbarComponent>() : nullptr;
}

void UBuildModeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsActive)
	{
		return;
	}

	RefreshPreviewMesh();
	UpdatePreviewTransform();

	if (bShowDebug && GEngine)
	{
		const FString ClassName = LastPreviewedClass ? LastPreviewedClass->GetName() : TEXT("(none)");
		GEngine->AddOnScreenDebugMessage(500, 0.f, FColor::Green, FString::Printf(TEXT("Build Mode: %s | rotation %d deg"), *ClassName, RotationSteps * FMath::RoundToInt(RotationStepDegrees)));
	}
}

void UBuildModeComponent::ToggleBuildMode()
{
	if (bIsActive)
	{
		ExitBuildMode();
	}
	else
	{
		EnterBuildMode();
	}
}

void UBuildModeComponent::EnterBuildMode()
{
	if (!HotbarComponent)
	{
		return;
	}

	bIsActive = true;
	RotationSteps = 0;

	// If the current hotbar selection isn't a buildable, snap it to the nearest one that is.
	HotbarComponent->EnsureValidSelection(/*bBuildablesOnly=*/true);

	if (!PreviewActor)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PreviewActor = GetWorld()->SpawnActor<AStaticMeshActor>(SpawnParams);

		if (PreviewActor)
		{
			if (UStaticMeshComponent* MeshComp = PreviewActor->GetStaticMeshComponent())
			{
				MeshComp->SetMobility(EComponentMobility::Movable);
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MeshComp->SetCastShadow(false);
			}

			LastPreviewedClass = nullptr;
		}
	}

	RefreshPreviewMesh();
}

void UBuildModeComponent::ExitBuildMode()
{
	bIsActive = false;
	RotationSteps = 0;

	if (PreviewActor)
	{
		PreviewActor->SetActorHiddenInGame(true);
	}
}

void UBuildModeComponent::RotatePreview(int32 Direction)
{
	if (!bIsActive || Direction == 0)
	{
		return;
	}

	const int32 StepsPerFullTurn = FMath::Max(1, FMath::RoundToInt(360.f / RotationStepDegrees));
	RotationSteps = (RotationSteps + FMath::Sign(Direction) + StepsPerFullTurn) % StepsPerFullTurn;
}

void UBuildModeComponent::ConfirmPlacement()
{
	if (!bIsActive || !HotbarComponent || !PreviewActor || PreviewActor->IsHidden())
	{
		return;
	}

	FHotbarEntry SelectedEntry;

	if (!HotbarComponent->GetSelectedEntry(SelectedEntry) || !SelectedEntry.ActorClass
		|| !SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass()))
	{
		return;
	}

	const FVector SpawnLocation = PreviewActor->GetActorLocation();
	const FRotator SpawnRotation = PreviewActor->GetActorRotation();

	const FHotbarEntry ConsumedEntry = HotbarComponent->ConsumeSelected();

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(ConsumedEntry.ActorClass, SpawnLocation, SpawnRotation, SpawnParams))
	{
		Spawned->SetActorScale3D(ConsumedEntry.Scale);
	}

	// The hotbar selection has already advanced (same-class continuation, or the next available
	// entry) - refresh right away so the ghost doesn't show a stale mesh for one frame.
	RefreshPreviewMesh();
}

void UBuildModeComponent::RefreshPreviewMesh()
{
	if (!HotbarComponent || !PreviewActor)
	{
		return;
	}

	FHotbarEntry SelectedEntry;
	const bool bHasBuildableSelected = HotbarComponent->GetSelectedEntry(SelectedEntry) && SelectedEntry.ActorClass
		&& SelectedEntry.ActorClass->IsChildOf(ABuildablePiece::StaticClass());

	if (!bHasBuildableSelected)
	{
		PreviewActor->SetActorHiddenInGame(true);
		LastPreviewedClass = nullptr;
		return;
	}

	PreviewActor->SetActorHiddenInGame(false);

	if (SelectedEntry.ActorClass == LastPreviewedClass)
	{
		// Same class as last frame - only the transform needs updating (done every tick separately).
		return;
	}

	LastPreviewedClass = SelectedEntry.ActorClass;

	const ABuildablePiece* CDO = SelectedEntry.ActorClass->GetDefaultObject<ABuildablePiece>();
	UStaticMeshComponent* MeshComp = PreviewActor->GetStaticMeshComponent();

	if (!CDO || !MeshComp)
	{
		return;
	}

	MeshComp->SetStaticMesh(CDO->GetMeshComponent() ? CDO->GetMeshComponent()->GetStaticMesh() : nullptr);

	for (int32 i = 0; i < MeshComp->GetNumMaterials(); ++i)
	{
		MeshComp->SetMaterial(i, GhostMaterial);
	}

	PreviewActor->SetActorScale3D(SelectedEntry.Scale);
}

void UBuildModeComponent::UpdatePreviewTransform()
{
	if (!HotbarComponent || !PreviewActor || PreviewActor->IsHidden())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;

	if (!OwnerController)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	FVector PreviewLocation = ViewLocation + ViewRotation.Vector() * HotbarComponent->PlaceDistance;
	PreviewLocation.X = FMath::GridSnap(PreviewLocation.X, HotbarComponent->GridSize);
	PreviewLocation.Y = FMath::GridSnap(PreviewLocation.Y, HotbarComponent->GridSize);
	PreviewLocation.Z = FMath::GridSnap(PreviewLocation.Z, HotbarComponent->GridSize);

	const FRotator PreviewRotation(0.f, RotationSteps * RotationStepDegrees, 0.f);

	PreviewActor->SetActorLocationAndRotation(PreviewLocation, PreviewRotation);
}

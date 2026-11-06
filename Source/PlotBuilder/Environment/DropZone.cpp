#include "Environment/DropZone.h"
#include "Destruction/DurabilityComponent.h"
#include "Environment/Plot.h"
#include "Kismet/GameplayStatics.h"
#include "Props/PhysicsProp.h"

void ADropZone::BeginPlay()
{
	Super::BeginPlay();

	if (APlot* Plot = Cast<APlot>(UGameplayStatics::GetActorOfClass(this, APlot::StaticClass())))
	{
		Plot->RegisterClientDropZone(this);
	}
}

bool ADropZone::Accepts(AActor* Actor) const
{
	return Cast<APhysicsProp>(Actor) != nullptr;
}

TArray<FDropItem> ADropZone::GetItems() const
{
	TArray<FDropItem> Items;

	for (AActor* Actor : OverlappingInteractables)
	{
		APhysicsProp* Prop = Cast<APhysicsProp>(Actor);

		// A prop destroyed since it entered: the end-overlap may not have run yet, the actor is already invalid.
		if (!IsValid(Prop))
		{
			continue;
		}

		FDropItem Item;
		Item.Prop = Prop;
		Item.Value = Prop->Value;

		if (const UDurabilityComponent* Durability = Prop->GetDurabilityComponent())
		{
			Item.HPRatio = Durability->GetHPRatio();
		}

		Items.Add(Item);
	}

	return Items;
}

int32 ADropZone::GetTotalValue() const
{
	int32 Total = 0;

	for (const FDropItem& Item : GetItems())
	{
		Total += Item.Value;
	}

	return Total;
}

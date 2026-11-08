#include "PaintBrushComponent.h"
#include "HotbarComponent.h"
#include "InteractionComponent.h"
#include "ToolbeltComponent.h"
#include "Buildables/BuildablePiece.h"
#include "Buildables/PaintPalette.h"
#include "Environment/Plot.h"
#include "Kismet/GameplayStatics.h"
#include "Props/PaintBucket.h"

UPaintBrushComponent::UPaintBrushComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPaintBrushComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();

	ToolbeltComponent = Owner ? Owner->FindComponentByClass<UToolbeltComponent>() : nullptr;
	HotbarComponent = Owner ? Owner->FindComponentByClass<UHotbarComponent>() : nullptr;
	InteractionComponent = Owner ? Owner->FindComponentByClass<UInteractionComponent>() : nullptr;

	// Single-plot levels for now, like build mode. No plot just means no palette, so the brush says so.
	CachedPlot = Cast<APlot>(UGameplayStatics::GetActorOfClass(GetWorld(), APlot::StaticClass()));
}

void UPaintBrushComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdatePrompt();
}

const UPaintPalette* UPaintBrushComponent::GetPalette() const
{
	const UPaintPalette* Palette = CachedPlot ? CachedPlot->PaintPalette : nullptr;
	return (Palette && Palette->Num() > 0) ? Palette : nullptr;
}

bool UPaintBrushComponent::GetBucketColor(TSubclassOf<AActor> BucketClass, FLinearColor& OutColor, FText& OutName) const
{
	const UPaintPalette* Palette = GetPalette();

	if (!Palette || !BucketClass || !BucketClass->IsChildOf(APaintBucket::StaticClass()))
	{
		return false;
	}

	const int32 Index = Palette->Wrap(BucketClass->GetDefaultObject<APaintBucket>()->PaletteIndex);
	OutColor = Palette->GetColor(Index);
	OutName = Palette->GetDisplayName(Index);
	return true;
}

UPaintBrushComponent::EBrushState UPaintBrushComponent::ResolveColor(int32& OutPaletteIndex) const
{
	const UPaintPalette* Palette = GetPalette();

	if (!Palette)
	{
		return EBrushState::NoPalette;
	}

	FInventorySlot Selected;

	if (!HotbarComponent || !HotbarComponent->GetSelectedEntry(Selected) || !Selected.ActorClass
		|| !Selected.ActorClass->IsChildOf(APaintBucket::StaticClass()))
	{
		return EBrushState::NoBucket;
	}

	OutPaletteIndex = Palette->Wrap(Selected.ActorClass->GetDefaultObject<APaintBucket>()->PaletteIndex);
	return EBrushState::Ready;
}

void UPaintBrushComponent::Say(const FText& Text, EMessageTone Tone) const
{
	if (UPlayerMessages* Messages = UPlayerMessages::Get(this))
	{
		Messages->Notify(Text, Tone);
	}
}

void UPaintBrushComponent::Paint()
{
	int32 PaletteIndex = INDEX_NONE;

	switch (ResolveColor(PaletteIndex))
	{
	case EBrushState::NoPalette:
		Say(NSLOCTEXT("PaintBrush", "NoPalette", "There is no paint palette on this plot."), EMessageTone::Error);
		return;

	case EBrushState::NoBucket:
		Say(NSLOCTEXT("PaintBrush", "NoBucket", "Select a paint bucket in the hotbar."), EMessageTone::Info);
		return;

	case EBrushState::Ready:
		break;
	}

	ABuildablePiece* Piece = InteractionComponent ? Cast<ABuildablePiece>(InteractionComponent->GetCurrentFocus()) : nullptr;

	// Only a piece standing in the house: one that is falling, or still in a stack, has nothing to wear paint.
	if (!Piece || Piece->State != EBuildablePieceState::Placed)
	{
		Say(NSLOCTEXT("PaintBrush", "NoTarget", "Aim at a placed piece to paint it."), EMessageTone::Info);
		return;
	}

	// The bucket is not used up for now. If it ever is, this is the one place to take the paint from it.
	Piece->ApplyPaint(PaletteIndex, GetPalette());
}

void UPaintBrushComponent::UpdatePrompt()
{
	const bool bHolding = ToolbeltComponent && ToolbeltComponent->IsBrushEquipped();

	if (!bHolding && !bPromptShown)
	{
		return;
	}

	UPlayerMessages* Messages = UPlayerMessages::Get(this);

	if (!Messages)
	{
		return;
	}

	if (!bHolding)
	{
		Messages->ClearPrompt(this);
		bPromptShown = false;
		return;
	}

	int32 PaletteIndex = INDEX_NONE;
	const EBrushState State = ResolveColor(PaletteIndex);

	// A palette entry is >= 0; the two missing-piece states get codes that no entry can have.
	const int32 Key = State == EBrushState::Ready ? PaletteIndex : (State == EBrushState::NoBucket ? -2 : -3);

	if (bPromptShown && Key == LastPromptKey)
	{
		return;
	}

	FText Line;

	switch (State)
	{
	case EBrushState::NoPalette:
		Line = NSLOCTEXT("PaintBrush", "PromptNoPalette", "Brush: this plot has no paint palette.");
		break;

	case EBrushState::NoBucket:
		Line = NSLOCTEXT("PaintBrush", "PromptNoBucket", "Brush: select a paint bucket in the hotbar.");
		break;

	case EBrushState::Ready:
	{
		FText ColorName = GetPalette()->GetDisplayName(PaletteIndex);

		if (ColorName.IsEmpty())
		{
			ColorName = FText::Format(NSLOCTEXT("PaintBrush", "UnnamedColor", "Colour {0}"), FText::AsNumber(PaletteIndex));
		}

		Line = FText::Format(NSLOCTEXT("PaintBrush", "PromptReady", "Brush: {0}  |  click to paint"), ColorName);
		break;
	}
	}

	Messages->SetPrompt(this, Line);
	bPromptShown = true;
	LastPromptKey = Key;
}

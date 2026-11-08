#include "ShopCartComponent.h"
#include "HotbarComponent.h"
#include "ShopItemRow.h"
#include "Framework/Application/SlateApplication.h"

UShopCartComponent::UShopCartComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UHotbarComponent* UShopCartComponent::GetHotbar() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UHotbarComponent>() : nullptr;
}

void UShopCartComponent::StepQuantity(FName RowName, int32 Direction)
{
	const int32 Step = FMath::Sign(Direction);

	if (Step == 0)
	{
		return;
	}

	// Read from Slate, not from the player controller: the menu runs in UI input mode, where the controller sees no keys.
	const bool bShift = FSlateApplication::IsInitialized() && FSlateApplication::Get().GetModifierKeys().IsShiftDown();

	SetQuantity(RowName, GetQuantity(RowName) + Step * (bShift ? FMath::Max(1, ShiftStep) : 1));
}

void UShopCartComponent::SetQuantity(FName RowName, int32 Quantity)
{
	UHotbarComponent* Hotbar = GetHotbar();
	const FShopItemRow* Row = (!RowName.IsNone() && Hotbar) ? Hotbar->FindRow(RowName) : nullptr;

	// Only rows of the catalog can be in the cart.
	if (!Row)
	{
		return;
	}

	// An item the player may hold only so many of (a paint bucket: one) cannot be added past what is left to buy.
	const int32 Limit = FMath::Min(FMath::Max(1, MaxQuantityPerLine), Hotbar->GetPurchaseLimit(*Row));
	Quantity = FMath::Clamp(Quantity, 0, Limit);

	const int32 Index = Lines.IndexOfByPredicate([RowName](const FCartLine& Line) { return Line.RowName == RowName; });

	if (Index == INDEX_NONE)
	{
		if (Quantity <= 0)
		{
			return;
		}

		FCartLine NewLine;
		NewLine.RowName = RowName;
		NewLine.Quantity = Quantity;
		Lines.Add(NewLine);
	}
	else if (Quantity <= 0)
	{
		Lines.RemoveAt(Index);
	}
	else if (Lines[Index].Quantity == Quantity)
	{
		return;
	}
	else
	{
		Lines[Index].Quantity = Quantity;
	}

	OnCartChanged.Broadcast();
}

void UShopCartComponent::Clear()
{
	if (Lines.IsEmpty())
	{
		return;
	}

	Lines.Reset();
	OnCartChanged.Broadcast();
}

bool UShopCartComponent::Confirm()
{
	UHotbarComponent* Hotbar = GetHotbar();

	if (!Hotbar || Lines.IsEmpty())
	{
		return false;
	}

	const int32 Units = Hotbar->QuoteLines(Lines).Units;

	if (!Hotbar->PurchaseLines(Lines))
	{
		return false;
	}

	Lines.Reset();
	OnCartChanged.Broadcast();
	OnPurchased.Broadcast(Units);
	return true;
}

int32 UShopCartComponent::GetQuantity(FName RowName) const
{
	const FCartLine* Line = Lines.FindByPredicate([RowName](const FCartLine& Candidate) { return Candidate.RowName == RowName; });
	return Line ? Line->Quantity : 0;
}

FCartQuote UShopCartComponent::GetQuote() const
{
	const UHotbarComponent* Hotbar = GetHotbar();
	return Hotbar ? Hotbar->QuoteLines(Lines) : FCartQuote();
}

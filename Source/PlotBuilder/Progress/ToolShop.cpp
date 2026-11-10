#include "ToolShop.h"

int32 ToolShop::ClampQuantity(const int32 Requested)
{
	return FMath::Clamp(Requested, 1, MaxQuantity);
}

FOfferStatus ToolShop::Evaluate(const FToolOfferRow& Row, const bool bOwned, const int32 Money, const bool bInfiniteMoney, const int32 Quantity)
{
	FOfferStatus Status;

	// A tool is bought once; a consumable is bought by the unit and can always be bought again.
	if (!Row.bConsumable && bOwned)
	{
		Status.State = EOfferState::Owned;
		return Status;
	}

	Status.Total = Row.Price * (Row.bConsumable ? ClampQuantity(Quantity) : 1);

	if (bInfiniteMoney || Money >= Status.Total)
	{
		Status.State = EOfferState::CanBuy;
		return Status;
	}

	Status.State = EOfferState::TooExpensive;
	Status.Missing = Status.Total - Money;
	return Status;
}

FParcelStatus ToolShop::EvaluateParcel(const int32 Price, const bool bOwned, const int32 Money, const bool bInfiniteMoney)
{
	FParcelStatus Status;

	if (bOwned)
	{
		Status.State = EParcelState::Owned;
		Status.Fraction = 1.f;
		return Status;
	}

	if (bInfiniteMoney || Money >= Price)
	{
		Status.State = EParcelState::CanBuy;
		Status.Fraction = 1.f;
		return Status;
	}

	Status.State = EParcelState::Saving;
	Status.Missing = Price - Money;
	Status.Fraction = Price > 0 ? FMath::Clamp(static_cast<float>(Money) / static_cast<float>(Price), 0.f, 1.f) : 1.f;
	return Status;
}

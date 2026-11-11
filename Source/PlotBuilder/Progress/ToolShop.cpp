#include "ToolShop.h"

int32 ToolShop::Room(const FToolOfferRow& Row, const int32 Stock)
{
	return Row.MaxStock > 0 ? FMath::Max(0, Row.MaxStock - Stock) : MaxQuantityUnlimited;
}

int32 ToolShop::ClampQuantity(const int32 Requested, const int32 RoomLeft)
{
	return FMath::Clamp(Requested, 1, FMath::Max(1, RoomLeft));
}

FOfferStatus ToolShop::Evaluate(const FToolOfferRow& Row, const bool bOwned, const int32 Money, const bool bInfiniteMoney, const int32 Quantity, const int32 Stock)
{
	FOfferStatus Status;

	// A tool is bought once; a consumable is bought by the unit and can be bought again, up to what the player may hold.
	if (!Row.bConsumable && bOwned)
	{
		Status.State = EOfferState::Owned;
		return Status;
	}

	if (Row.bConsumable && Room(Row, Stock) <= 0)
	{
		Status.State = EOfferState::AtLimit;
		return Status;
	}

	Status.Total = Row.Price * (Row.bConsumable ? ClampQuantity(Quantity, Room(Row, Stock)) : 1);

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

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ToolShop.generated.h"

/**
 *  One thing the PC sells in its Tools tab. The row name is the tool id the save keeps ("Crowbar"), so adding a tool to the
 *  shop is a row in DT_ToolShop, not new code.
 */
USTRUCT(BlueprintType)
struct FToolOfferRow : public FTableRowBase
{
	GENERATED_BODY()

	/** The slot of UToolbeltComponent this tool sits in. It gives the card its icon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	int32 BeltSlot = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	FText DisplayName;

	/** One line on what the tool is for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	FText Blurb;

	/** The materials it is good against, one chip each ("Wood", "Concrete"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	TArray<FText> GoodAgainst;

	/** What the clicks do, in a short sentence ("Left click: swing."). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	FText Clicks;

	/** The price of one tool, or of one charge. 0 for what the player always has (the hands). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer", meta = (ClampMin = "0"))
	int32 Price = 0;

	/** Used up by playing: bought by the unit and never shown as owned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer")
	bool bConsumable = false;

	/** The most of this consumable the player may hold at once (the charges in stock). The stepper and the purchase stop there. 0 = no limit. Ignored for a tool. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Offer", meta = (ClampMin = "0"))
	int32 MaxStock = 10;
};

enum class EOfferState : uint8
{
	/** A tool the player already has. */
	Owned,
	/** The player can pay for it now. */
	CanBuy,
	/** The player cannot pay yet: Missing says how much is short. */
	TooExpensive,
	/** A consumable of which the player already holds as many as the row allows. */
	AtLimit,
};

struct FOfferStatus
{
	EOfferState State = EOfferState::CanBuy;

	/** What buying the quantity asked for would cost. */
	int32 Total = 0;

	/** How much money is short for it, 0 unless TooExpensive. */
	int32 Missing = 0;
};

enum class EParcelState : uint8
{
	Owned,
	CanBuy,
	/** Not enough yet: the tab shows how far along the player is. */
	Saving,
};

struct FParcelStatus
{
	EParcelState State = EParcelState::Saving;

	/** How much money is short, 0 unless Saving. */
	int32 Missing = 0;

	/** Money over price, 0 to 1, for the progress bar. */
	float Fraction = 0.f;
};

/**
 *  What the purchase screens say, as pure functions of plain numbers, so they are tested without a level or a widget.
 *  With infinite money (the hotbar's switch, for tests and the demo) nothing is ever too expensive.
 */
namespace ToolShop
{
	/** The most of one consumable the stepper goes to when its row sets no limit (MaxStock 0). */
	constexpr int32 MaxQuantityUnlimited = 99;

	/** How many more of this consumable the player may take: what is left under the row's MaxStock, or MaxQuantityUnlimited with no limit. 0 = the stock is full. */
	int32 Room(const FToolOfferRow& Row, int32 Stock);

	/** The quantity kept between 1 and RoomLeft (never below 1, so that the stepper still shows a number when the stock is full). */
	int32 ClampQuantity(int32 Requested, int32 RoomLeft);

	/** What a card says. Quantity and Stock only matter for a consumable: Stock is what the player holds, and caps the quantity at the room left. */
	FOfferStatus Evaluate(const FToolOfferRow& Row, bool bOwned, int32 Money, bool bInfiniteMoney, int32 Quantity = 1, int32 Stock = 0);

	/** What the Parcel tab says. */
	FParcelStatus EvaluateParcel(int32 Price, bool bOwned, int32 Money, bool bInfiniteMoney);
}

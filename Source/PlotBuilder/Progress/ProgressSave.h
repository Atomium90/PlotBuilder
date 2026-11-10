#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ProgressSave.generated.h"

class AActor;
class ABuildablePiece;

/**
 *  One kit piece of the player's home, as the shared placement function takes it (APlot::PlacePiece): class, grid
 *  cell, quarter turns, roof step and the scale it was placed with. Scale is the one PlacePiece received, not the
 *  actor's current one, which carries the corner and wall-clearance biases and would be biased twice on restore.
 *
 *  These records have their own struct, with SaveGame on every field, because a save only writes the properties
 *  that carry the flag (FInventorySlot's do not).
 */
USTRUCT()
struct FHomePieceRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftClassPtr<ABuildablePiece> PieceClass;

	UPROPERTY(SaveGame)
	FIntVector Cell = FIntVector::ZeroValue;

	UPROPERTY(SaveGame)
	uint8 QuarterTurns = 0;

	UPROPERTY(SaveGame)
	int32 RoofStep = 0;

	UPROPERTY(SaveGame)
	FVector Scale = FVector::OneVector;

	/** Index in the paint palette, 0 = unpainted. */
	UPROPERTY(SaveGame)
	int32 PaintIndex = 0;
};

/** One loose prop of the home (furniture put down in the world): class and transform in the plot's space, so the plot can move in the level. */
USTRUCT()
struct FHomePropRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(SaveGame)
	FTransform PlotRelativeTransform;
};

/** One slot of the player's inventory, as UHotbarComponent keeps it. */
USTRUCT()
struct FHomeSlotRecord
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(SaveGame)
	FVector Scale = FVector::OneVector;

	UPROPERTY(SaveGame)
	int32 Count = 0;

	UPROPERTY(SaveGame)
	int32 StackSize = 0;
};

/**
 *  What the player left in Lvl_Home (GDD §12, D24): the pieces, the props and the inventory. Slots keeps the grid
 *  position (index = row * columns + column), empty slots included, so the inventory comes back as it was.
 *  bValid is false until a first snapshot is taken: a new game restores nothing.
 */
USTRUCT()
struct FHomeSnapshot
{
	GENERATED_BODY()

	UPROPERTY(SaveGame)
	bool bValid = false;

	UPROPERTY(SaveGame)
	TArray<FHomePieceRecord> Pieces;

	UPROPERTY(SaveGame)
	TArray<FHomePropRecord> Props;

	UPROPERTY(SaveGame)
	TArray<FHomeSlotRecord> Slots;
};

/**
 *  What the player keeps from one session to the next (GDD §18): the money, the tools bought, whether the parcel
 *  is owned, and the snapshot of the home. UProgressSubsystem holds the one live copy and writes it to the
 *  "Progress" slot.
 */
UCLASS()
class UProgressSave : public USaveGame
{
	GENERATED_BODY()

public:

	/** Money a new game starts with (and the sledgehammer). */
	static constexpr int32 DefaultMoney = 500;

	UPROPERTY(SaveGame)
	int32 Money = DefaultMoney;

	/**
	 *  Ids of the tools the player owns (the row names of DT_ToolShop). The tool belt reads this on the maps that
	 *  enforce ownership (UToolbeltComponent::SetOwnershipEnforced); the PC's Tools tab adds to it.
	 */
	UPROPERTY(SaveGame)
	TSet<FName> OwnedTools;

	/** Explosive charges in stock. Unlike the tools they are used up: every throw or placement takes one. */
	UPROPERTY(SaveGame)
	int32 Charges = 0;

	UPROPERTY(SaveGame)
	bool bParcelOwned = false;

	/** The player's home, written when they leave Lvl_Home and restored when they come back (AHomeDirector). */
	UPROPERTY(SaveGame)
	FHomeSnapshot Home;

	/** A new game: DefaultMoney, the hands and the sledgehammer, no charges, no parcel, no home. */
	void ResetToDefaults();

	/** Only the tools: the hands and the sledgehammer, no charges. Money, parcel and home stay. */
	void ResetTools();
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PaintPalette.generated.h"

/** One colour of the palette: the name the player reads, and the colour the piece is tinted with. */
USTRUCT(BlueprintType)
struct FPaintColor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	FLinearColor Color = FLinearColor::White;
};

/**
 *  The colours the paint brush can apply (build mode only). Pure data: a new colour is a new row, never new code.
 *
 *  Entry 0 is the neutral one: a piece that wears it shows the material's own colour, whatever Color says there. That
 *  makes a piece that was never painted and a piece painted "neutral" the same thing, and lets the saved index default
 *  to 0 everywhere. Painting with entry 0 therefore removes a paint.
 *
 *  A piece wears an index, not a colour, so the colours can be retuned here without touching a save. The colour reaches
 *  the material through the mesh's Custom Primitive Data: RGB at ColorIndex, how much of it to mix in at BlendIndex.
 *  Indices 0 (stability) and 1 (damage tint) belong to ABuildablePiece and are never written by the paint.
 */
UCLASS(BlueprintType)
class UPaintPalette : public UDataAsset
{
	GENERATED_BODY()

public:

	/** First Custom Primitive Data slot of the colour (R, G, B in ColorIndex to ColorIndex + 2). M_Greybox reads these. */
	static constexpr int32 ColorIndex = 2;

	/** Custom Primitive Data slot of the blend: 0 shows the material's colour, 1 shows the paint. M_Greybox reads this one. */
	static constexpr int32 BlendIndex = 5;

	/** Entry 0 must be the neutral one (see the class comment). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Paint")
	TArray<FPaintColor> Colors;

	int32 Num() const { return Colors.Num(); }

	/** Index brought back into [0, Num()) by wrapping around, so stepping past either end loops. 0 for an empty palette. */
	int32 Wrap(int32 Index) const
	{
		const int32 Count = Colors.Num();
		return Count > 0 ? ((Index % Count) + Count) % Count : 0;
	}

	/** True for entry 0, and for any index the palette does not have (nothing to show, so nothing is tinted). */
	bool IsNeutral(int32 Index) const { return Index == 0 || !Colors.IsValidIndex(Index); }

	FLinearColor GetColor(int32 Index) const { return Colors.IsValidIndex(Index) ? Colors[Index].Color : FLinearColor::White; }

	FText GetDisplayName(int32 Index) const { return Colors.IsValidIndex(Index) ? Colors[Index].DisplayName : FText::GetEmpty(); }
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "ClientRow.generated.h"

class UTexture2D;

/**
 *  One client of the board (a row of DT_Clients, the row name is the client id the pool draws). The board, the notebook and the
 *  debrief show the name, the role and the portrait; Block 3 gives clients a personality, which will be more columns here. Adding
 *  a client is a new row, not new code.
 */
USTRUCT(BlueprintType)
struct FClientRow : public FTableRowBase
{
	GENERATED_BODY()

	/** "Brandt & Sons". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Client")
	FText DisplayName;

	/** What they do, under the name: "Developer", "Landlord". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Client")
	FText Role;

	/** The portrait. Empty draws a silhouette on the tint below (the real portraits come in Block 3). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Client")
	TSoftObjectPtr<UTexture2D> Portrait;

	/** The colour behind the silhouette, so two clients without a portrait are still told apart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Client")
	FLinearColor Tint = FLinearColor(0.23f, 0.29f, 0.39f, 1.f);
};

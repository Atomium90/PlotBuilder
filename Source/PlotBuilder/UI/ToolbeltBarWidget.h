#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ToolbeltBarWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UUIStyle;
class UToolbeltComponent;
class UToolTileWidget;

/**
 *  The tool belt on screen: one tile per tool the player owns, plus one padlock tile for the ones missing. In the HUD it is
 *  a column on the left; in the bag drawer it is a row of larger tiles. Which it is comes from the widget Blueprint (the
 *  container the tiles go into and the tile class), nothing here depends on it.
 *
 *  When the equipped tool changes, its own tile shows a chip with its name and what the two clicks do for a few seconds,
 *  then it goes away by itself, so it takes no room while the player works.
 */
UCLASS(Abstract)
class UToolbeltBarWidget : public UUserWidget
{
	GENERATED_BODY()

protected:

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(EditDefaultsOnly, Category = "Tool Belt")
	TObjectPtr<UUIStyle> Style;

	UPROPERTY(EditDefaultsOnly, Category = "Tool Belt")
	TSubclassOf<UToolTileWidget> TileClass;

	/** Seconds the equipped-tool label stays up. Zero keeps it hidden (the bag drawer shows names on the tiles). */
	UPROPERTY(EditDefaultsOnly, Category = "Tool Belt")
	float LabelSeconds = 5.f;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> TileHost;

private:

	bool TryBind();
	void Unbind();
	void BuildTiles();
	void RefreshTiles();
	void ShowLabel();
	void HideLabel();

	UFUNCTION()
	void HandleBeltChanged();

	UPROPERTY()
	TObjectPtr<UToolbeltComponent> Belt;

	UPROPERTY()
	TArray<TObjectPtr<UToolTileWidget>> Tiles;

	bool bBound = false;
	int32 LastEquipped = INDEX_NONE;
	FTimerHandle LabelTimer;
};

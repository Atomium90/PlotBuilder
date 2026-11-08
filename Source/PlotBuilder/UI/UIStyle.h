#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateBrush.h"
#include "Interaction/ShopItemRow.h"
#include "UIStyle.generated.h"

class UTexture2D;

/** What one shop category looks like: the colour its items wear, and its icon. */
USTRUCT(BlueprintType)
struct FUICategoryStyle
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Category")
	TObjectPtr<UTexture2D> Icon;
};

/**
 *  The look shared by the shop, the inventory, the hotbar and the tool belt: the colours, the category colours and icons,
 *  the tool icons. One Data Asset instead of colours scattered over the widget Blueprints, so a retune is one place.
 *
 *  Colours are written in sRGB (what an editor colour picker shows) and converted, so the values here match the mock-ups.
 */
UCLASS(BlueprintType)
class UUIStyle : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Accent = Srgb(255, 122, 47);

	/** Text on top of the accent colour (buttons, badges). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor AccentText = Srgb(26, 14, 5);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor TextPrimary = Srgb(232, 237, 245);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor TextMuted = Srgb(154, 166, 184);

	/** A number that blocks something (a total the player cannot pay). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Danger = Srgb(255, 154, 138);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Success = Srgb(127, 214, 160);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor PanelBackground = Srgb(28, 34, 44);

	/** Side rails and the cart panel, a step darker than the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor RailBackground = Srgb(22, 27, 35);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor FieldBackground = Srgb(20, 25, 34);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor Line = Srgb(47, 58, 77);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor RowHover = Srgb(34, 42, 55);

	/** A selected button, a shop row with something in the cart. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor RowActive = Srgb(37, 45, 58);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor SlotBackground = Srgb(22, 27, 35);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Colors")
	FLinearColor SlotBorder = Srgb(52, 64, 90);

	/** The colour and the icon of each shop category. A category missing here falls back to the primary text colour. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Categories")
	TMap<EShopItemCategory, FUICategoryStyle> Categories;

	/** The padlock of the case that stands for the tools the player does not own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TObjectPtr<UTexture2D> LockIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TObjectPtr<UTexture2D> SearchIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TObjectPtr<UTexture2D> CloseIcon;

	/** One icon per tool belt slot, in slot order (Hands, Charge, Sledgehammer, Crowbar, Jackhammer, Brush). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Icons")
	TArray<TObjectPtr<UTexture2D>> ToolIcons;

	FLinearColor GetCategoryColor(EShopItemCategory Category) const;

	UTexture2D* GetCategoryIcon(EShopItemCategory Category) const;

	UTexture2D* GetToolIcon(int32 ToolSlot) const;

	/** The category as the player reads it ("Structure"). */
	static FText GetCategoryLabel(EShopItemCategory Category);

	/** The same colour with another opacity. */
	static FLinearColor WithAlpha(const FLinearColor& Color, float Alpha);

	/** Black or white, whichever reads on top of Background. */
	static FLinearColor InkFor(const FLinearColor& Background);

	/** A rounded rectangle brush: the fill, the corner radius, and an optional outline. */
	static FSlateBrush MakeRounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f);

	static FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255) { return FLinearColor::FromSRGBColor(FColor(R, G, B, A)); }
};

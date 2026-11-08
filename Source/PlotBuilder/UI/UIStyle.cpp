#include "UIStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"

FLinearColor UUIStyle::GetCategoryColor(EShopItemCategory Category) const
{
	const FUICategoryStyle* Style = Categories.Find(Category);
	return Style ? Style->Color : TextPrimary;
}

UTexture2D* UUIStyle::GetCategoryIcon(EShopItemCategory Category) const
{
	const FUICategoryStyle* Style = Categories.Find(Category);
	return Style ? Style->Icon.Get() : nullptr;
}

UTexture2D* UUIStyle::GetToolIcon(int32 ToolSlot) const
{
	return ToolIcons.IsValidIndex(ToolSlot) ? ToolIcons[ToolSlot].Get() : nullptr;
}

FText UUIStyle::GetCategoryLabel(EShopItemCategory Category)
{
	switch (Category)
	{
	case EShopItemCategory::Structure:
		return NSLOCTEXT("UIStyle", "Structure", "Structure");
	case EShopItemCategory::Opening:
		return NSLOCTEXT("UIStyle", "Opening", "Opening");
	case EShopItemCategory::Roof:
		return NSLOCTEXT("UIStyle", "Roof", "Roof");
	case EShopItemCategory::Prop:
		return NSLOCTEXT("UIStyle", "Prop", "Prop");
	case EShopItemCategory::Paint:
		return NSLOCTEXT("UIStyle", "Paint", "Paint");
	default:
		return FText::GetEmpty();
	}
}

FLinearColor UUIStyle::WithAlpha(const FLinearColor& Color, float Alpha)
{
	return FLinearColor(Color.R, Color.G, Color.B, Alpha);
}

FLinearColor UUIStyle::InkFor(const FLinearColor& Background)
{
	// Perceived brightness of the colour as the eye sees it (sRGB), 0.6 and up reads best with dark text.
	const FColor Display = Background.ToFColorSRGB();
	const float Brightness = (0.299f * Display.R + 0.587f * Display.G + 0.114f * Display.B) / 255.f;
	return Brightness > 0.6f ? Srgb(20, 19, 15) : FLinearColor::White;
}

FSlateBrush UUIStyle::MakeRounded(const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth)
{
	return FSlateRoundedBoxBrush(Fill, Radius, Outline, OutlineWidth);
}

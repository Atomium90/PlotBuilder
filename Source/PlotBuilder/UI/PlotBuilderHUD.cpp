#include "UI/PlotBuilderHUD.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "UI/PlayerMessages.h"

namespace
{
	// File-private names are unique across the module (unity build).

	FLinearColor HudToneColor(EMessageTone Tone)
	{
		switch (Tone)
		{
		case EMessageTone::Error:
			return FLinearColor(1.f, 0.35f, 0.3f);
		case EMessageTone::Warning:
			return FLinearColor(1.f, 0.75f, 0.3f);
		default:
			return FLinearColor::White;
		}
	}
}

void APlotBuilderHUD::DrawHUD()
{
	Super::DrawHUD();

	UPlayerMessages* Messages = UPlayerMessages::Get(this);
	const UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	if (!Messages || !Canvas || !Font)
	{
		return;
	}

	const float Scale = (Canvas->ClipY / ReferenceHeight) * TextScale;
	const float LineHeight = Font->GetMaxCharHeight() * Scale * LineSpacing;
	const float CentreX = Canvas->ClipX * 0.5f;

	const auto DrawCentred = [&](const FText& Text, const FLinearColor& Color, float Y)
	{
		FCanvasTextItem Item(FVector2D(CentreX, Y), Text, Font, Color);
		Item.bCentreX = true;
		Item.Scale = FVector2D(Scale, Scale);
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f), FVector2D(2.f, 2.f));
		Canvas->DrawItem(Item);
	};

	if (!Messages->GetPrompt().IsEmpty())
	{
		DrawCentred(Messages->GetPrompt(), FLinearColor(1.f, 0.9f, 0.4f), Canvas->ClipY * PromptHeightShare);
	}

	// Oldest on top, so a new message appears at the bottom of the stack.
	float Y = Canvas->ClipY * ToastHeightShare;

	for (const FPlayerToast& Toast : Messages->GetToasts())
	{
		DrawCentred(Toast.Text, HudToneColor(Toast.Tone), Y);
		Y += LineHeight;
	}
}

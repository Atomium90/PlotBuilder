#include "ShopWidget.h"
#include "CartLineWidget.h"
#include "ShopCategoryButton.h"
#include "ShopRowWidget.h"
#include "UIStyle.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GameFramework/Pawn.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "PlotBuilder.h"
#include "Interaction/HotbarComponent.h"
#include "Interaction/PaintBrushComponent.h"
#include "Interaction/ShopCartComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	// File-private names are unique across the module (unity build).

	/** Shop categories after the "all" entry, in the order of EShopItemCategory. */
	constexpr int32 ShopWidgetCategoryCount = 5;

	FText ShopWidgetInfinity()
	{
		return FText::FromString(FString(TEXT("∞")));
	}
}

void UShopWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SearchBox->OnTextChanged.AddDynamic(this, &UShopWidget::HandleSearchChanged);
	SortNameButton->OnClicked.AddDynamic(this, &UShopWidget::HandleSortName);
	SortPriceButton->OnClicked.AddDynamic(this, &UShopWidget::HandleSortPrice);
	ClearButton->OnClicked.AddDynamic(this, &UShopWidget::HandleClear);
	ConfirmButton->OnClicked.AddDynamic(this, &UShopWidget::HandleConfirm);

	if (SearchClearButton)
	{
		SearchClearButton->OnClicked.AddDynamic(this, &UShopWidget::HandleSearchClear);
	}
}

void UShopWidget::NativeConstruct()
{
	Super::NativeConstruct();

	TryBind();
}

void UShopWidget::NativeDestruct()
{
	Unbind();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DoneTimer);
	}

	Super::NativeDestruct();
}

void UShopWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The pawn may not exist yet when the HUD is built; the first frame the menu is up finishes the job.
	if (!bBound)
	{
		TryBind();
	}
}

bool UShopWidget::TryBind()
{
	if (bBound)
	{
		return true;
	}

	const APawn* Pawn = GetOwningPlayerPawn();

	if (!Pawn)
	{
		return false;
	}

	Hotbar = Pawn->FindComponentByClass<UHotbarComponent>();
	Cart = Pawn->FindComponentByClass<UShopCartComponent>();
	Brush = Pawn->FindComponentByClass<UPaintBrushComponent>();

	if (!Hotbar || !Cart || !Style || !Hotbar->ShopCatalog)
	{
		// The pawn exists but something the menu needs does not: say so once instead of showing an empty shop.
		UE_LOG(LogPlotBuilder, Warning, TEXT("UShopWidget: cannot build the shop (hotbar %s, cart %s, style %s, catalog %s)"),
			Hotbar ? TEXT("ok") : TEXT("missing"), Cart ? TEXT("ok") : TEXT("missing"), Style ? TEXT("ok") : TEXT("missing"),
			(Hotbar && Hotbar->ShopCatalog) ? TEXT("ok") : TEXT("missing"));
		return false;
	}

	Hotbar->OnMoneyChanged.AddUniqueDynamic(this, &UShopWidget::HandleMoneyChanged);
	Hotbar->OnInventoryChanged.AddUniqueDynamic(this, &UShopWidget::HandleInventoryChanged);
	Cart->OnCartChanged.AddUniqueDynamic(this, &UShopWidget::HandleCartChanged);
	Cart->OnPurchased.AddUniqueDynamic(this, &UShopWidget::HandlePurchased);

	if (CategoryButtons.IsEmpty())
	{
		BuildCategories();
	}

	if (Rows.IsEmpty())
	{
		BuildRows();
	}

	bBound = true;

	if (DoneText)
	{
		DoneText->SetText(FText::GetEmpty());
	}

	RefreshSortButtons();
	RefreshList();
	RefreshOwned();
	RefreshBag();
	RefreshCart();
	return true;
}

void UShopWidget::Unbind()
{
	if (Hotbar)
	{
		Hotbar->OnMoneyChanged.RemoveDynamic(this, &UShopWidget::HandleMoneyChanged);
		Hotbar->OnInventoryChanged.RemoveDynamic(this, &UShopWidget::HandleInventoryChanged);
	}

	if (Cart)
	{
		Cart->OnCartChanged.RemoveDynamic(this, &UShopWidget::HandleCartChanged);
		Cart->OnPurchased.RemoveDynamic(this, &UShopWidget::HandlePurchased);
	}

	bBound = false;
}

void UShopWidget::BuildCategories()
{
	if (!CategoryButtonClass)
	{
		return;
	}

	CategoryList->ClearChildren();

	TArray<int32> Counts;
	Counts.Init(0, ShopWidgetCategoryCount);

	for (const FName RowName : Hotbar->ShopCatalog->GetRowNames())
	{
		const FShopItemRow* Row = Hotbar->FindRow(RowName);

		if (Row && Row->ActorClass && Counts.IsValidIndex(static_cast<int32>(Row->Category)))
		{
			++Counts[static_cast<int32>(Row->Category)];
		}
	}

	int32 Total = 0;

	for (const int32 Count : Counts)
	{
		Total += Count;
	}

	for (int32 FilterIndex = 0; FilterIndex <= ShopWidgetCategoryCount; ++FilterIndex)
	{
		UShopCategoryButton* Button = CreateWidget<UShopCategoryButton>(this, CategoryButtonClass);

		if (!Button)
		{
			continue;
		}

		const bool bAll = FilterIndex == 0;
		const EShopItemCategory Category = bAll ? EShopItemCategory::Structure : static_cast<EShopItemCategory>(FilterIndex - 1);

		Button->Setup(Style, FilterIndex, bAll, Category, bAll ? Total : Counts[FilterIndex - 1]);
		Button->SetActive(FilterIndex == ActiveFilter);
		Button->OnPicked.AddUObject(this, &UShopWidget::HandleCategoryPicked);

		CategoryList->AddChild(Button);
		CategoryButtons.Add(Button);
	}
}

void UShopWidget::BuildRows()
{
	if (!RowClass)
	{
		return;
	}

	for (const FName RowName : Hotbar->ShopCatalog->GetRowNames())
	{
		const FShopItemRow* Row = Hotbar->FindRow(RowName);

		if (!Row || !Row->ActorClass)
		{
			continue;
		}

		if (UShopRowWidget* RowWidget = CreateWidget<UShopRowWidget>(this, RowClass))
		{
			RowWidget->Setup(Style, RowName, *Row, Cart, Hotbar, Brush);
			Rows.Add(RowWidget);
		}
	}

	ApplyOrder();
}

void UShopWidget::ApplyOrder()
{
	TArray<UShopRowWidget*> Ordered;

	for (UShopRowWidget* RowWidget : Rows)
	{
		Ordered.Add(RowWidget);
	}

	const ESort ActiveSort = Sort;

	// Sort on an array of pointers hands the predicate the objects pointed to, not the pointers.
	Ordered.Sort([ActiveSort](const UShopRowWidget& A, const UShopRowWidget& B)
	{
		if (ActiveSort == ESort::Price && A.GetRow().Cost != B.GetRow().Cost)
		{
			return A.GetRow().Cost < B.GetRow().Cost;
		}

		return A.GetRow().DisplayName.CompareToCaseIgnored(B.GetRow().DisplayName) < 0;
	});

	// Taking a row out of the panel and putting it back builds its Slate widgets again, which is slow with 72 rows.
	// So the rows stay in the panel for good: this is only called when the sort changes, and filtering is RefreshList.
	RowList->ClearChildren();

	for (UShopRowWidget* RowWidget : Ordered)
	{
		RowList->AddChild(RowWidget);
	}
}

void UShopWidget::RefreshList()
{
	const FString Needle = Query.TrimStartAndEnd().ToLower();
	int32 Shown = 0;

	for (UShopRowWidget* RowWidget : Rows)
	{
		const FShopItemRow& Row = RowWidget->GetRow();

		const bool bCategoryOk = ActiveFilter == 0 || static_cast<int32>(Row.Category) == ActiveFilter - 1;
		const bool bSearchOk = Needle.IsEmpty() || Row.DisplayName.ToString().ToLower().Contains(Needle);
		const bool bMatch = bCategoryOk && bSearchOk;

		// A filter only shows or hides: the row stays built.
		RowWidget->SetVisibility(bMatch ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		Shown += bMatch ? 1 : 0;
	}

	if (ShownCountText)
	{
		ShownCountText->SetText(FText::Format(NSLOCTEXT("Shop", "ItemsShown", "{0} items shown"), FText::AsNumber(Shown)));
	}
}

void UShopWidget::RefreshOwned()
{
	for (UShopRowWidget* RowWidget : Rows)
	{
		RowWidget->RefreshOwned();
	}
}

void UShopWidget::RefreshBag()
{
	if (!Hotbar)
	{
		return;
	}

	const int32 Total = Hotbar->GetTotalSlotCount();
	const int32 Used = Total - Hotbar->GetFreeSlotCount();

	if (BagText)
	{
		BagText->SetText(FText::Format(NSLOCTEXT("Shop", "BagSlots", "{0} of {1} slots"), FText::AsNumber(Used), FText::AsNumber(Total)));
	}

	if (BagBar)
	{
		BagBar->SetPercent(Total > 0 ? static_cast<float>(Used) / Total : 0.f);
	}
}

void UShopWidget::RefreshSortButtons()
{
	if (!Style)
	{
		return;
	}

	const bool bByName = Sort == ESort::Name;

	if (SortNameBackground)
	{
		SortNameBackground->SetColorAndOpacity(bByName ? Style->Accent : FLinearColor::Transparent);
	}

	if (SortPriceBackground)
	{
		SortPriceBackground->SetColorAndOpacity(bByName ? FLinearColor::Transparent : Style->Accent);
	}

	if (SortNameLabel)
	{
		SortNameLabel->SetColorAndOpacity(FSlateColor(bByName ? Style->AccentText : Style->TextPrimary));
	}

	if (SortPriceLabel)
	{
		SortPriceLabel->SetColorAndOpacity(FSlateColor(bByName ? Style->TextPrimary : Style->AccentText));
	}
}

void UShopWidget::RefreshMoney()
{
	RefreshCart();
}

void UShopWidget::RefreshCart()
{
	if (!Hotbar || !Cart || !Style)
	{
		return;
	}

	CartLineList->ClearChildren();

	const TArray<FCartLine>& Lines = Cart->GetLines();

	if (CartLineClass)
	{
		for (const FCartLine& Line : Lines)
		{
			const FShopItemRow* Row = Hotbar->FindRow(Line.RowName);
			UCartLineWidget* LineWidget = Row ? CreateWidget<UCartLineWidget>(this, CartLineClass) : nullptr;

			if (LineWidget)
			{
				LineWidget->Setup(Style, Line.RowName, *Row, Line.Quantity, Cart, Brush);
				CartLineList->AddChild(LineWidget);
			}
		}
	}

	CartEmptyText->SetVisibility(Lines.IsEmpty() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);

	const FCartQuote Quote = Cart->GetQuote();
	const bool bInfinite = Hotbar->IsInfiniteMoney();

	CartUnitsText->SetText(FText::AsNumber(Quote.Units));
	TotalText->SetText(FText::AsNumber(Quote.TotalCost));
	BalanceText->SetText(bInfinite ? ShopWidgetInfinity() : FText::AsNumber(Hotbar->GetMoney()));
	AfterText->SetText(bInfinite ? ShopWidgetInfinity() : FText::AsNumber(Hotbar->GetMoney() - Quote.TotalCost));
	AfterText->SetColorAndOpacity(FSlateColor(Quote.bAffordable ? Style->TextPrimary : Style->Danger));

	BagSpaceText->SetText(FText::Format(NSLOCTEXT("Shop", "BagSpace", "{0} needed, {1} free"),
		FText::AsNumber(Quote.SlotsNeeded), FText::AsNumber(Quote.FreeSlots)));
	BagSpaceText->SetColorAndOpacity(FSlateColor(Quote.bFits ? Style->TextPrimary : Style->Danger));

	ClearButton->SetIsEnabled(!Lines.IsEmpty());
	ConfirmButton->SetIsEnabled(Quote.CanConfirm());

	FText Label = FText::Format(NSLOCTEXT("Shop", "ConfirmPurchase", "Confirm purchase  |  {0}"), FText::AsNumber(Quote.TotalCost));

	if (Quote.IsEmpty())
	{
		Label = NSLOCTEXT("Shop", "CartEmpty", "Cart is empty");
	}
	else if (!Quote.bAffordable)
	{
		Label = FText::Format(NSLOCTEXT("Shop", "NeedMore", "Need {0} more"), FText::AsNumber(Quote.TotalCost - Hotbar->GetMoney()));
	}
	else if (!Quote.bFits)
	{
		Label = NSLOCTEXT("Shop", "NoRoom", "Not enough bag space");
	}

	ConfirmLabel->SetText(Label);

	for (UShopRowWidget* RowWidget : Rows)
	{
		RowWidget->RefreshCart();
	}
}

void UShopWidget::Close()
{
	OnCloseRequested.Broadcast();
}

FReply UShopWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey Key = InKeyEvent.GetKey();
	// The focus sits on the text inside the box, so ask for descendants as well.
	const bool bTyping = SearchBox && (SearchBox->HasKeyboardFocus() || SearchBox->HasFocusedDescendants());

	// The key that opened the menu may still be held. Its auto-repeat must not close the menu again: the controller
	// would see the key as a new press and reopen it, and the screen would flicker for as long as the key is down.
	if (InKeyEvent.IsRepeat() && !bTyping && (Key == EKeys::Escape || Key == EKeys::B))
	{
		return FReply::Handled();
	}

	if (Key == EKeys::Escape)
	{
		// In the search field Escape first empties it, so the player does not lose the menu while looking for something.
		if (bTyping && !SearchBox->GetText().IsEmpty())
		{
			SearchBox->SetText(FText::GetEmpty());
		}
		else
		{
			Close();
		}

		return FReply::Handled();
	}

	// B closes the shop it opened, except while a letter is being typed in the search field.
	if (!bTyping && Key == EKeys::B)
	{
		Close();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UShopWidget::HandleSearchChanged(const FText& NewText)
{
	Query = NewText.ToString();

	// The cross only shows while there is something to clear.
	if (SearchClearButton)
	{
		SearchClearButton->SetVisibility(Query.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}

	RefreshList();
}

void UShopWidget::HandleSearchClear()
{
	SearchBox->SetText(FText::GetEmpty());

	// SetText does not fire OnTextChanged, so the list would keep the last filter until the next keystroke.
	HandleSearchChanged(FText::GetEmpty());
}

void UShopWidget::HandleSortName()
{
	Sort = ESort::Name;
	RefreshSortButtons();
	ApplyOrder();
}

void UShopWidget::HandleSortPrice()
{
	Sort = ESort::Price;
	RefreshSortButtons();
	ApplyOrder();
}

void UShopWidget::HandleCategoryPicked(int32 FilterIndex)
{
	ActiveFilter = FilterIndex;

	// The buttons were added in the order of the filter index, so the position in the rail is the index.
	for (int32 Index = 0; Index < CategoryButtons.Num(); ++Index)
	{
		CategoryButtons[Index]->SetActive(Index == ActiveFilter);
	}

	RefreshList();
}

void UShopWidget::HandleClear()
{
	if (Cart)
	{
		Cart->Clear();
	}
}

void UShopWidget::HandleConfirm()
{
	if (Cart)
	{
		Cart->Confirm();
	}
}

void UShopWidget::HandleCartChanged()
{
	RefreshCart();
}

void UShopWidget::HandlePurchased(int32 Units)
{
	if (!DoneText || !Style)
	{
		return;
	}

	DoneText->SetText(FText::Format(NSLOCTEXT("Shop", "Purchased", "Purchased {0} items"), FText::AsNumber(Units)));
	DoneText->SetColorAndOpacity(FSlateColor(Style->Success));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(DoneTimer, FTimerDelegate::CreateUObject(this, &UShopWidget::ClearDoneText), PurchaseMessageSeconds, false);
	}
}

void UShopWidget::ClearDoneText()
{
	if (DoneText)
	{
		DoneText->SetText(FText::GetEmpty());
	}
}

void UShopWidget::HandleMoneyChanged()
{
	RefreshMoney();
}

void UShopWidget::HandleInventoryChanged()
{
	RefreshOwned();
	RefreshBag();
	RefreshCart();
}

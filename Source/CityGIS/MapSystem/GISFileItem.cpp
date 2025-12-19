#include "GISFileItem.h"
#include "GISLoadDialog.h"

void UGISFileItem::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_Select) Btn_Select->OnClicked.AddDynamic(this, &UGISFileItem::OnClicked);
}

void UGISFileItem::Setup(const FGISSaveMetadata& Meta, UGISLoadDialog* Parent)
{
	SaveData = Meta;
	ParentDialog = Parent;

	if (Txt_Name) Txt_Name->SetText(FText::FromString(Meta.Name));
	if (Txt_Date) Txt_Date->SetText(FText::FromString(Meta.Date));
	SetSelected(false);
}

void UGISFileItem::SetSelected(bool bSelected)
{
	if (Bg_Border)
	{
		// 选中变蓝，未选中透明或灰色
		Bg_Border->SetBrushColor(bSelected ? FLinearColor(0.0f, 0.2f, 0.5f, 1.0f) : FLinearColor(0.1f, 0.1f, 0.1f, 0.5f));
	}
}

void UGISFileItem::OnClicked()
{
	if (ParentDialog)
	{
		ParentDialog->OnItemSelected(this);
	}
}
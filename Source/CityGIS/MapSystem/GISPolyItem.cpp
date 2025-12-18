#include "GISPolyItem.h"
#include "GISWebWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"

void UGISPolyItem::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Focus) Btn_Focus->OnClicked.AddDynamic(this, &UGISPolyItem::OnFocusClicked);
	if (Btn_Delete) Btn_Delete->OnClicked.AddDynamic(this, &UGISPolyItem::OnDeleteClicked);
	// 【新增】绑定编辑按钮
	if (Btn_Edit) Btn_Edit->OnClicked.AddDynamic(this, &UGISPolyItem::OnEditClicked);
}

void UGISPolyItem::SetupItem(FString InID, FString InName, FString InType, FString InParentID, FString InColor,
                             float InOpacity, FString InTextColor, FString InTag, UGISWebWidget* InMainUI)
{
	this->ItemID = InID;
	this->ItemName = InName;
	this->ItemType = InType;
	this->ItemColor = InColor;
	this->ItemOpacity = InOpacity;
	this->ItemTextColor = InTextColor;
	this->MainUI = InMainUI;
	this->ItemTag = InTag;
	// 1. 保存标签

	if (Txt_Name) Txt_Name->SetText(FText::FromString(InName));

	// 2. 设置类型文本、背景色和缩进
	FLinearColor BgColor = FLinearColor::Gray;
	float Indent = 0.0f;
	FString DisplayType = InType;

	if (InType == "District")
	{
		BgColor = FLinearColor(0.8f, 0.1f, 0.1f, 0.6f); // 红
		Indent = 0.0f;
		DisplayType = TEXT("区镇");
	}
	else if (InType == "Street")
	{
		BgColor = FLinearColor(0.1f, 0.1f, 0.8f, 0.6f); // 蓝
		Indent = 30.0f;
		DisplayType = TEXT("街道");
	}
	else if (InType == "Community")
	{
		BgColor = FLinearColor(0.1f, 0.6f, 0.1f, 0.6f); // 绿
		Indent = 60.0f;
		DisplayType = TEXT("小区");
	}
	else if (InType == "Reconstruct")
	{
		BgColor = FLinearColor(0.5f, 0.0f, 0.5f, 0.6f); // 紫
		Indent = 0.0f;
		DisplayType = TEXT("重构");
	}
	else if (InType == "Custom") // 3. 新增自定义类型样式
	{
		BgColor = FLinearColor(0.0f, 0.5f, 0.5f, 0.6f); // 青色 (区分)
		Indent = 60.0f; // 缩进与小区一致 (因为属于街道)
		DisplayType = TEXT("自定义");

		// 如果有标签，显示为 "自定义 | 标签名"
		if (!InTag.IsEmpty())
		{
			DisplayType += TEXT(" | ") + InTag;
		}
	}

	if (Txt_Type) Txt_Type->SetText(FText::FromString(DisplayType));

	// 4. 应用缩进
	if (Content_Border)
	{
		Content_Border->SetBrushColor(BgColor);
		Content_Border->SetPadding(FMargin(Indent, 2.0f, 0.0f, 2.0f));
	}
}

void UGISPolyItem::AddChildItem(UGISPolyItem* ChildWidget)
{
	if (Child_Container && ChildWidget)
	{
		Child_Container->AddChild(ChildWidget);
	}
}

void UGISPolyItem::UpdateData(FString NewName, FString NewColor, float NewOpacity, FString NewTextColor)
{
	this->ItemName = NewName;
	this->ItemColor = NewColor;
	this->ItemOpacity = NewOpacity;
	this->ItemTextColor = NewTextColor;

	if (Txt_Name) Txt_Name->SetText(FText::FromString(NewName));
}

void UGISPolyItem::OnFocusClicked()
{
	if (MainUI.IsValid()) MainUI->FocusID(ItemID);
}

void UGISPolyItem::OnDeleteClicked()
{
	if (MainUI.IsValid()) MainUI->DeleteID(ItemID);
	this->RemoveFromParent();
}

void UGISPolyItem::OnEditClicked()
{
	if (MainUI.IsValid()) MainUI->OpenEditDialog(this);
}

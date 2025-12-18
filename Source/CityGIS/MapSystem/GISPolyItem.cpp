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

	if (Txt_Name) Txt_Name->SetText(FText::FromString(InName));

	// --- 1. 确定显示名称和样式 ---
	FString DisplayType = InType;
	FLinearColor BgColor = FLinearColor::Gray;
	float Indent = 0.0f;

	if (InType == "District")
	{
		DisplayType = TEXT("区镇");
		BgColor = FLinearColor(0.8f, 0.1f, 0.1f, 0.6f); // 红
		Indent = 0.0f;
	}
	else if (InType == "Street")
	{
		DisplayType = TEXT("街道");
		BgColor = FLinearColor(0.1f, 0.1f, 0.8f, 0.6f); // 蓝
		Indent = 30.0f;
	}
	else if (InType == "Community")
	{
		DisplayType = TEXT("小区");
		BgColor = FLinearColor(0.1f, 0.6f, 0.1f, 0.6f); // 绿
		Indent = 60.0f;
	}
	else if (InType == "Reconstruct")
	{
		DisplayType = TEXT("重构");
		BgColor = FLinearColor(0.5f, 0.0f, 0.5f, 0.6f); // 紫
		Indent = 0.0f;
	}
	else if (InType == "Custom")
	{
		DisplayType = TEXT("自定义");
		BgColor = FLinearColor(0.0f, 0.5f, 0.5f, 0.6f); // 青
		Indent = 60.0f; // 缩进同小区
	}

	// --- 2. 【核心修复】统一追加标签 ---
	// 无论是什么类型，只要有 Tag，就显示出来
	if (!InTag.IsEmpty())
	{
		DisplayType += TEXT(" | ") + InTag;
	}

	// --- 3. 应用到 UI ---
	if (Txt_Type) Txt_Type->SetText(FText::FromString(DisplayType));

	if (Content_Border)
	{
		// Content_Border->SetBrushColor(BgColor);
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

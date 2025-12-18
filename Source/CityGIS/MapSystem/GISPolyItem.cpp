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
	this->MainUI = InMainUI;
	this->ItemTag = InTag;

	if (Txt_Name)
		Txt_Name->SetText(FText::FromString(InName));

	if (Txt_Type)
		Txt_Type->SetText(FText::FromString(InType)); // 比如显示 "街道"

	// 如果是自定义区域，可以在 UI 上显示一下 Tag (可选)
	if (InType == "Custom" && !InTag.IsEmpty() && Txt_Type)
	{
		Txt_Type->SetText(FText::FromString(InTag)); // 用 Tag 替换类型显示
	}

	// 设置类型文本和颜色/缩进
	FLinearColor BgColor = FLinearColor::Gray;
	float Indent = 0.0f;

	if (InType == "District")
	{
		BgColor = FLinearColor(0.8f, 0.1f, 0.1f, 0.6f); // 红
		Indent = 0.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("区镇")));
	}
	else if (InType == "Street")
	{
		BgColor = FLinearColor(0.1f, 0.1f, 0.8f, 0.6f); // 蓝
		Indent = 30.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("街道")));
	}
	else if (InType == "Community")
	{
		BgColor = FLinearColor(0.1f, 0.6f, 0.1f, 0.6f); // 绿
		Indent = 60.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("小区")));
	}
	else if (InType == "Reconstruct")
	{
		BgColor = FLinearColor(0.5f, 0.0f, 0.5f, 0.6f); // 紫
		Indent = 0.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("重构")));
	}

	// 应用缩进 (这里假设 Content_Border 是最外层容器)
	if (Content_Border)
	{
		Content_Border->SetBrushColor(BgColor);
		// 设置左边距实现层级感
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

void UGISPolyItem::UpdateData(FString NewName, FString NewColor, float NewOpacity)
{
	// 1. 更新内部变量
	this->ItemName = NewName;
	this->ItemColor = NewColor;
	this->ItemOpacity = NewOpacity;

	// 2. 【核心修复】立即更新 UI 上的文字
	if (Txt_Name)
	{
		Txt_Name->SetText(FText::FromString(NewName));
	}

	// (可选) 如果你希望列表项的背景色也跟着变，可以在这里写：
	if (Content_Border)
	{
		Content_Border->SetBrushColor(FLinearColor::FromSRGBColor(FColor::FromHex(NewColor)));
	}
}

void UGISPolyItem::OnFocusClicked()
{
	if (MainUI.IsValid()) MainUI->FocusID(ItemID);
}

void UGISPolyItem::OnDeleteClicked()
{
	if (MainUI.IsValid()) MainUI->DeleteID(ItemID);
	// 注意：这里不需要 RemoveFromParent，因为 JS 会回调 Delete 逻辑，
	// 或者你可以直接删，但为了同步最好是等 JS 确认删除了再通过刷新列表移除。
	// 为了简单体验，直接移除：
	this->RemoveFromParent();
}

void UGISPolyItem::OnEditClicked()
{
	if (MainUI.IsValid())
	{
		// 呼叫主界面的编辑弹窗
		MainUI->OpenEditDialog(this);
	}
}

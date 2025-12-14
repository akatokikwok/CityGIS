#include "GISPolyItem.h"
#include "GISWebWidget.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"

void UGISPolyItem::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_Focus) Btn_Focus->OnClicked.AddDynamic(this, &UGISPolyItem::OnFocusClicked);
	if (Btn_Delete) Btn_Delete->OnClicked.AddDynamic(this, &UGISPolyItem::OnDeleteClicked);
}

void UGISPolyItem::SetupItem(FString InID, FString InName, FString InType, UGISWebWidget* InMainUI)
{
	this->ItemID = InID;
	this->ItemType = InType;
	this->MainUI = InMainUI;

	if (Txt_Name) Txt_Name->SetText(FText::FromString(InName));
	
	// 设置类型文本和颜色/缩进
	FLinearColor BgColor = FLinearColor::Gray;
	float Indent = 0.0f;

	if (InType == "District") {
		BgColor = FLinearColor(0.8f, 0.1f, 0.1f, 0.6f); // 红
		Indent = 0.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("区镇")));
	}
	else if (InType == "Street") {
		BgColor = FLinearColor(0.1f, 0.1f, 0.8f, 0.6f); // 蓝
		Indent = 30.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("街道")));
	}
	else if (InType == "Community") {
		BgColor = FLinearColor(0.1f, 0.6f, 0.1f, 0.6f); // 绿
		Indent = 60.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("小区")));
	}
	else if (InType == "Reconstruct") {
		BgColor = FLinearColor(0.5f, 0.0f, 0.5f, 0.6f); // 紫
		Indent = 0.0f;
		if (Txt_Type) Txt_Type->SetText(FText::FromString(TEXT("重构")));
	}

	// 应用缩进 (这里假设 Content_Border 是最外层容器)
	if (Content_Border) {
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
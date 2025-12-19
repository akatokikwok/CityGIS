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

void UGISPolyItem::SetupItem(FString InID, FString InName, FString InType, FString InParentID, FString InColor, float InOpacity, FString InTextColor, FString InTag, float InHeight, UGISWebWidget* InMainUI)
{
    this->ItemID = InID;
    this->ItemName = InName;
    this->ItemType = InType;
    this->ItemColor = InColor;
    this->ItemOpacity = InOpacity;
    this->ItemTextColor = InTextColor;
    this->ItemTag = InTag;
    this->ItemHeight = InHeight; // 【新增】保存高度
    this->MainUI = InMainUI;

    if (Txt_Name)
    {
        Txt_Name->SetText(FText::FromString(InName));
    }
    
    // --- 1. 处理类型显示逻辑 ---
    FString DisplayType = InType;
    
    if (InType == "District") 
    {
        DisplayType = TEXT("区镇");
    }
    else if (InType == "Street") 
    {
        DisplayType = TEXT("街道");
    }
    else if (InType == "Community") 
    {
        DisplayType = TEXT("小区");
    }
    else if (InType == "Reconstruct") 
    {
        DisplayType = TEXT("重构");
    }
    else if (InType == "Custom") 
    {
        DisplayType = TEXT("自定义");
    }
    else if (InType == "Road") // 【新增】道路类型显示
    {
        DisplayType = TEXT("道路");
    }

    // 【新增】拼接标签 (Tag)
    if (!InTag.IsEmpty()) 
    {
        DisplayType += TEXT(" | ") + InTag;
    }
    
    // 【新增】拼接高度 (Height)，如果有值的话
    if (InHeight > 0.0f)
    {
        // 显示为 " | H:20m"
        DisplayType += FString::Printf(TEXT(" | H:%.0fm"), InHeight);
    }

    if (Txt_Type)
    {
        Txt_Type->SetText(FText::FromString(DisplayType));
    }

    // --- 2. 设置颜色条与缩进 ---
    if (Content_Border)
    {
        float Indent = 0.0f;
        FLinearColor BgColor = FLinearColor::Gray;

        if (InType == "District") 
        {
            BgColor = FLinearColor(0.8f, 0.1f, 0.1f, 0.6f);
        } 
        else if (InType == "Street") 
        {
            BgColor = FLinearColor(0.1f, 0.1f, 0.8f, 0.6f);
            Indent = 20.0f;
        } 
        else if (InType == "Community") 
        {
            BgColor = FLinearColor(0.1f, 0.6f, 0.1f, 0.6f);
            Indent = 40.0f;
        } 
        else if (InType == "Custom") 
        {
            BgColor = FLinearColor(0.0f, 0.5f, 0.5f, 0.6f);
            Indent = 40.0f;
        } 
        else if (InType == "Road") // 【新增】道路样式
        {
            BgColor = FLinearColor(0.2f, 0.2f, 0.2f, 0.8f); // 深灰色背景
            Indent = 40.0f; // 与小区同级缩进
        }

        Content_Border->SetBrushColor(BgColor);
        
        if (Child_Container)
        {
            Child_Container->SetRenderTranslation(FVector2D(Indent, 0));
        }
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

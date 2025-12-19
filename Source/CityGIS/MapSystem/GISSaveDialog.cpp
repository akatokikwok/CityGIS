#include "GISSaveDialog.h"
#include "GISWebWidget.h"
#include "Misc/DateTime.h"

void UGISSaveDialog::NativeConstruct()
{
	Super::NativeConstruct();
	if (Btn_Confirm) Btn_Confirm->OnClicked.AddDynamic(this, &UGISSaveDialog::OnConfirm);
	if (Btn_Cancel) Btn_Cancel->OnClicked.AddDynamic(this, &UGISSaveDialog::OnCancel);
}

void UGISSaveDialog::Init(UGISWebWidget* Parent, FString InJsonData)
{
	MainUI = Parent;
	PendingJsonData = InJsonData;
    
	// 默认给个名字：存档_日期
	if (Input_Name)
	{
		FString DefaultName = FString::Printf(TEXT("Save_%s"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M")));
		Input_Name->SetText(FText::FromString(DefaultName));
	}
	if (Input_Desc) Input_Desc->SetText(FText::GetEmpty());
    
	AddToViewport(100); // 确保显示在最上层
}

void UGISSaveDialog::OnConfirm()
{
	if (MainUI)
	{
		FString Name = Input_Name ? Input_Name->GetText().ToString() : "Unamed";
		FString Desc = Input_Desc ? Input_Desc->GetText().ToString() : "";
		MainUI->ExecuteSaveToFile(PendingJsonData, Name, Desc);
	}
	RemoveFromParent();
}

void UGISSaveDialog::OnCancel()
{
	RemoveFromParent();
}
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/EditableText.h"
#include "Components/MultiLineEditableText.h"
#include "Components/Button.h"
#include "GISSaveDialog.generated.h"

class UGISWebWidget;

UCLASS()
class CITYGIS_API UGISSaveDialog : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
    
	// 打开弹窗，传入等待保存的 JSON 数据
	void Init(UGISWebWidget* Parent, FString InJsonData);

protected:
	UPROPERTY(meta = (BindWidget)) UEditableText* Input_Name;
	UPROPERTY(meta = (BindWidget)) UMultiLineEditableText* Input_Desc;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Confirm;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Cancel;

private:
	UFUNCTION() void OnConfirm();
	UFUNCTION() void OnCancel();

	UPROPERTY()
	UGISWebWidget* MainUI = nullptr;
	
	FString PendingJsonData;
};
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "GISLoadDialog.generated.h"

class UGISWebWidget;
class UGISFileItem;

UCLASS()
class CITYGIS_API UGISLoadDialog : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
    
	void Init(UGISWebWidget* Parent);
	void OnItemSelected(UGISFileItem* Item);

protected:
	UPROPERTY(meta = (BindWidget)) UScrollBox* FileList;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Load;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Cancel;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Delete; // 可选：删除存档功能

	// 列表项类
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UGISFileItem> FileItemClass;

private:
	void RefreshList();
	UFUNCTION() void OnLoadClicked();
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnDeleteClicked();

	UPROPERTY()
	UGISWebWidget* MainUI = nullptr;
	
	UPROPERTY()
	UGISFileItem* SelectedItem = nullptr;
};
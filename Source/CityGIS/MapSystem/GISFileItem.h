#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "GISWebWidget.h" // 引用结构体
#include "GISFileItem.generated.h"

class UGISLoadDialog;

UCLASS()
class CITYGIS_API UGISFileItem : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeConstruct() override;
    
	// 初始化数据
	void Setup(const FGISSaveMetadata& Meta, UGISLoadDialog* Parent);
    
	// 选中状态切换
	void SetSelected(bool bSelected);

	FString GetFilePath() const { return SaveData.FilePath; }

	const FGISSaveMetadata& GetSaveData() const { return SaveData; }
protected:
	UPROPERTY(meta = (BindWidget)) UTextBlock* Txt_Name;
	UPROPERTY(meta = (BindWidget)) UTextBlock* Txt_Date;
	UPROPERTY(meta = (BindWidget)) UButton* Btn_Select;
	UPROPERTY(meta = (BindWidget)) UBorder* Bg_Border; // 用于高亮背景

private:
	UFUNCTION() void OnClicked();
    
	FGISSaveMetadata SaveData;

	UPROPERTY()
	UGISLoadDialog* ParentDialog = nullptr;
};
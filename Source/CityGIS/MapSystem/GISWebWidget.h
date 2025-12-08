/*
 * 这个类负责“遥控”网页，并把网页吐出的数据写死在硬盘上。
 */

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GISWebWidget.generated.h"

class UWebBrowser;

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UWebBrowser* MainBrowser = nullptr; // 必须与蓝图命名一致

	virtual void NativeConstruct() override;

	// 功能接口
	UFUNCTION(BlueprintCallable)
	void ToggleMode(bool bIsEdit);
	UFUNCTION(BlueprintCallable)
	void SaveData();
	UFUNCTION(BlueprintCallable)
	void LoadData();
	UFUNCTION(BlueprintCallable)
	void AddMarkerAtCenter(FString Type);

private:
	UFUNCTION()
	void OnTitleChanged(const FText& TitleText);
	
	FString SavePath = TEXT("");
};

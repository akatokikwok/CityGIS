/*
 * 这个类负责“遥控”网页，并把网页吐出的数据写死在硬盘上。
 */

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GISWebWidget.generated.h"

class UWebBrowser;

// 代理：通知蓝图加列表
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAddPolyItem, FString, ID, FString, Name);

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FOnAddPolyItem OnAddPolyItem;

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- 核心功能接口 (供蓝图调用) ---
	UFUNCTION(BlueprintCallable)
	void SetMode(FString ModeName); // "browse", "edit", "snap"

	UFUNCTION(BlueprintCallable)
	void SaveMapAs(FString FileName);

	UFUNCTION(BlueprintCallable)
	void LoadMap(FString FileName);

	UFUNCTION(BlueprintCallable)
	TArray<FString> GetSaveFiles();

	UFUNCTION(BlueprintCallable)
	void FocusID(FString ID);
	
	UFUNCTION(BlueprintCallable)
	void DeleteID(FString ID);

	UFUNCTION(BlueprintCallable)
	void UndoAction();

	UFUNCTION(BlueprintCallable)
	void SearchByName(FString Name);

	UFUNCTION(BlueprintCallable)
	void ActivateReconstructionTool();

	// 绑定蓝图里的 Web Browser 组件，名字必须叫 MapBrowser
	UPROPERTY(meta = (BindWidget))
	UWebBrowser* MapBrowser = nullptr;

private:
	// 监听 JS 发来的数据
	UFUNCTION()
	void OnTitleChanged(const FText& TitleText);

	//
	UFUNCTION()
	void OnJSMessageRecv(const FString& Message, const FString& Source, int32 Line);

	FString SaveFilePath;
};

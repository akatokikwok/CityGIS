/*
 * 这个类负责“遥控”网页，并把网页吐出的数据写死在硬盘上。
 */

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GISWebWidget.generated.h"

class UWebBrowser;

// 【修改】增加 Type 参数
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnAddPolyItem, FString, ID, FString, Name, FString, Type);

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FOnAddPolyItem OnAddPolyItemDelegate;

	// 绑定蓝图里的 Web Browser 组件，名字必须叫 MapBrowser
	UPROPERTY(meta = (BindWidget))
	UWebBrowser* MapBrowser = nullptr;
	
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

	// 【新增】筛选功能
	UFUNCTION(BlueprintCallable)
	void FilterByType(FString TypeName); // "all", "Normal", "Custom"...
	
private:
	UFUNCTION()
	void OnTitleChanged(const FText& TitleText);

	// 监听 JS 发来的数据链条
	UFUNCTION()
	void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

	FString SaveFilePath;

	FString LastProcessedID; 
	double LastLogTime = 0.0f; // 用于防止微秒级重复
};

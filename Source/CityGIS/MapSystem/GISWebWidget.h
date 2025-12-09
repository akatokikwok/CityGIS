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
	// 绑定蓝图里的 Web Browser 组件，名字必须叫 MapBrowser
	UPROPERTY(meta = (BindWidget))
	UWebBrowser* MapBrowser;

	virtual void NativeConstruct() override;

	// --- 核心功能接口 (供蓝图调用) ---
	UFUNCTION(BlueprintCallable) void SetMode(FString ModeName); // "browse", "edit", "snap"
	UFUNCTION(BlueprintCallable) void SaveMap();
	UFUNCTION(BlueprintCallable) void LoadMap();
	UFUNCTION(BlueprintCallable) void UndoAction();
	UFUNCTION(BlueprintCallable) void SearchByName(FString Name);

private:
	UFUNCTION() void OnTitleChanged(const FText& TitleText); // 监听 JS 发来的数据
	FString SaveFilePath;
};

/*
 * 这个类负责“遥控”网页，并把网页吐出的数据写死在硬盘上。
 */

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "GISWebWidget.generated.h"

class UGISPolyItem;
class UWebBrowser;

// 【修改】增加 ParentID 参数 (共4个参数)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnAddPolyItem, FString, ID, FString, Name, FString, Type, FString,
                                              ParentID);

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintAssignable)
	FOnAddPolyItem OnAddPolyItemDelegate;

	// 绑定蓝图里的 Web Browser 组件，名字必须叫 MapBrowser
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UWebBrowser* MapBrowser = nullptr;

	// 两个列表容器
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* List_Admin;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* List_Reconstruct;

	// UI 控件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UEditableText* Input_SaveName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UComboBoxString* Combo_Files;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UComboBoxString* Combo_Filter;

	// --- 资源引用 ---
	// 在蓝图 Class Defaults 里设置这个，指向 WBP_PolyItem
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UGISPolyItem> PolyItemClass;

	// 【新增 UI】编辑弹窗容器 (Overlay 或 CanvasPanel)
	UPROPERTY(meta = (BindWidget))
	UWidget* Edit_Dialog_Overlay;

	// 【新增 UI】编辑弹窗里的输入控件
	UPROPERTY(meta = (BindWidget))
	UEditableText* Edit_Input_Name;

	UPROPERTY(meta = (BindWidget))
	UEditableText* Edit_Input_Color; // 简单起见用 Hex 文本框

	UPROPERTY(meta = (BindWidget))
	UEditableText* Edit_Input_Opacity; // 用文本框输 0.5 简单

	// 【新增 UI】颜色相关控件
	UPROPERTY(meta = (BindWidget)) UBorder* Color_Preview;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_R;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_G;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_B;
	
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

	// 【新增】打开编辑弹窗
	void OpenEditDialog(class UGISPolyItem* ItemToEdit);

	// 【新增】保存编辑 (供蓝图按钮调用)
	UFUNCTION(BlueprintCallable)
	void SaveEditChanges();

	// 【新增】关闭编辑弹窗
	UFUNCTION(BlueprintCallable)
	void CloseEditDialog();

	// 预设按钮建议在蓝图里绑定事件调用 C++ 函数，或者用 BindWidgetOptional
	// 这里我们用通用函数处理预设点击
	UFUNCTION(BlueprintCallable)
	void OnPresetColorClicked(FLinearColor NewColor);
	
private:
	UFUNCTION()
	void OnTitleChanged(const FText& TitleText);

	// 监听 JS 发来的数据链条
	UFUNCTION()
	void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

	// 核心：处理新条目创建和层级挂载
	// 修改函数声明，增加 Tag 参数
	void ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color, float Opacity, FString TextColor, FString Tag);

	// 内部函数：当滑块变动时更新 Hex 和 预览
	UFUNCTION() void OnColorSliderChanged(float Value);

	// 辅助：更新 UI 显示 (Hex 文本, 预览图, 滑块位置)
	void UpdateColorUI(FLinearColor Color);
	
	// 字典：快速查找父级 Widget
	TMap<FString, UGISPolyItem*> WidgetMap;

	FString SaveFilePath;

	FString LastProcessedID;
	double LastLogTime = 0.0f; // 用于防止微秒级重复
	
	// 当前正在编辑的 Item 指针
	TWeakObjectPtr<UGISPolyItem> CurrentEditingItem;
};

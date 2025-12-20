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
#include "GISLoadDialog.h"
#include "GISSaveDialog.h"
#include "GISWebWidget.generated.h"

class UGISPolyItem;
class UWebBrowser;


// 【新增】存档数据结构体 (对应单个存档文件)
USTRUCT(BlueprintType)
struct FGISSaveMetadata
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FString ID;          // 唯一GUID

	UPROPERTY(BlueprintReadWrite)
	FString Name;        // 存档名

	UPROPERTY(BlueprintReadWrite)
	FString Description; // 备注

	UPROPERTY(BlueprintReadWrite)
	FString Date;        // 存档日期

	UPROPERTY(BlueprintReadWrite)
	FString FilePath;    // 文件物理路径 (仅读取时填充)
    
	// 注意：具体的 Polygon JSON 数据不在这里显示，只在底层读写
};




// 【修改】增加 ParentID 参数 (共4个参数)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnAddPolyItem, FString, ID, FString, Name, FString, Type, FString,
                                              ParentID);

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- 存档/读写核心接口 ---

	// --- 核心功能 ---
	UFUNCTION(BlueprintCallable) void RequestSaveDataFromWeb(); // 点击主界面"存档"按钮
	UFUNCTION(BlueprintCallable) void OpenLoadDialog();         // 点击主界面"读档"按钮
	
	// 2. 执行保存：当JS数据传回，且用户在弹窗填完信息后调用此函数写入磁盘
	// GeoJsonData: 从JS获取的原始数据
	// SaveName, SaveDesc: 用户在UI输入的
	UFUNCTION(BlueprintCallable)
	void ExecuteSaveToFile(FString GeoJsonData, FString SaveName, FString SaveDesc);

	void ExecuteLoadFromFile(FString FilePath);
	
	// 3. 获取所有存档列表：用于读档弹窗显示
	UFUNCTION(BlueprintCallable)
	TArray<FGISSaveMetadata> GetAllSaveFiles();

	// 4. 执行读档：用户在列表选中某一项后调用
	UFUNCTION(BlueprintCallable)
	void LoadSaveFile(FString FilePath);
	
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
	// 在 protected 区域，List_Reconstruct 下方添加：
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UScrollBox* List_Road;
	
	// --- 资源引用 ---
	// 在蓝图 Class Defaults 里设置这个，指向 WBP_PolyItem
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UGISPolyItem> PolyItemClass;

	// --- 【新增】弹窗类配置 ---

	// UGISSaveDialog类型的蓝图控件
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UGISSaveDialog> SaveDialogClass;

	// UGISLoadDialog类型的蓝图控件
	UPROPERTY(EditAnywhere, Category = "Config")
	TSubclassOf<UGISLoadDialog> LoadDialogClass;
	
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

	UPROPERTY(meta = (BindWidget)) UEditableText* Edit_Input_TextColor;
	
	// 【新增 UI】颜色相关控件
	UPROPERTY(meta = (BindWidget)) UBorder* Color_Preview;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_R;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_G;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_B;

	UPROPERTY(meta = (BindWidget)) UBorder* TextColor_Preview;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_R;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_G;
	UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_B;

	// 【新增】蓝图事件：当JS返回了全部地图数据，通知蓝图打开“保存详情输入框”
	// 参数：RawJsonData (地图原始数据)
	UFUNCTION(BlueprintImplementableEvent)
	void OnMapDataReceivedForSave(const FString& RawJsonData);
	
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
	// 【修改】增加 float Height 参数，用于接收高架/建筑高度
	void ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color, float Opacity, FString TextColor, FString Tag, float Height);

	// 内部函数：当滑块变动时更新 Hex 和 预览
	UFUNCTION() void OnColorSliderChanged(float Value);

	// 辅助：更新 UI 显示 (Hex 文本, 预览图, 滑块位置)
	void UpdateColorUI(FLinearColor Color);

	//
	void UpdateTextColorUI(FLinearColor Color);
	
	// 字典：快速查找父级 Widget
	UPROPERTY()
	TMap<FString, UGISPolyItem*> WidgetMap;

	FString SaveFilePath;

	FString LastProcessedID;
	double LastLogTime = 0.0f; // 用于防止微秒级重复
	
	// 当前正在编辑的 Item 指针
	TWeakObjectPtr<UGISPolyItem> CurrentEditingItem;
};

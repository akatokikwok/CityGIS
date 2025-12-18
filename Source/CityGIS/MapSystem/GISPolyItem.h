#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "GISPolyItem.generated.h"

// 前向声明，防止循环引用
class UGISWebWidget;

UCLASS()
class CITYGIS_API UGISPolyItem : public UUserWidget
{
	GENERATED_BODY()

	friend class UGISWebWidget;

public:
	virtual void NativeConstruct() override;

	// 【修改】SetupItem 增加颜色和透明度参数
	void SetupItem(FString InID, FString InName, FString InType, FString InParentID, FString InColor, float InOpacity, FString InTextColor, FString InTag, UGISWebWidget* InMainUI);

	// 添加子项到下方的容器中
	void AddChildItem(UGISPolyItem* ChildWidget);

	// 获取当前属性供编辑弹窗使用
	FString GetItemID() const { return ItemID; }
	FString GetItemName() const { return ItemName; }
	FString GetItemColor() const { return ItemColor; }
	float GetItemOpacity() const { return ItemOpacity; }
	FString GetItemTextColor() const { return ItemTextColor; }
	
	// 【新增】更新数据接口
	void UpdateData(FString NewName, FString NewColor, float NewOpacity, FString NewTextColor);
	
protected:
	// 【新增】编辑按钮
	UPROPERTY(meta = (BindWidget))
	UButton* Btn_Edit;

	// --- UI 绑定 (名字必须和 UMG 里的一样) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Txt_Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UTextBlock* Txt_Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* Btn_Focus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UButton* Btn_Delete;

	// 核心：用来放子级 (街道/小区)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UVerticalBox* Child_Container;

	// 用来控制缩进的容器 (通常是包裹内容的 Border 或 HorizontalBox)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
	UBorder* Content_Border;

private:
	UFUNCTION()
	void OnFocusClicked();
	UFUNCTION()
	void OnDeleteClicked();
	UFUNCTION()
	void OnEditClicked(); // 新增点击事件

	FString ItemID;
	FString ItemName; // 存一下名字
	FString ItemType;
	FString ItemColor; // 存颜色 Hex
	FString ItemTextColor;
	float ItemOpacity; // 存透明度

	// 弱引用主界面，防止内存泄漏
	TWeakObjectPtr<UGISWebWidget> MainUI;

	FString ItemTag; // 存储标签
};

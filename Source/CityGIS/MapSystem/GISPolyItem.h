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

	// 初始化数据 (代替蓝图的 Expose on Spawn)
	void SetupItem(FString InID, FString InName, FString InType, UGISWebWidget* InMainUI);

	// 添加子项到下方的容器中
	void AddChildItem(UGISPolyItem* ChildWidget);

protected:
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
	UFUNCTION() void OnFocusClicked();
	UFUNCTION() void OnDeleteClicked();

	FString ItemID;
	FString ItemType;
	
	// 弱引用主界面，防止内存泄漏
	TWeakObjectPtr<UGISWebWidget> MainUI;
};
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/Border.h"
#include "GISPolyItem.generated.h"

class UGISWebWidget;

UCLASS()
class CITYGIS_API UGISPolyItem : public UUserWidget
{
    GENERATED_BODY()
    friend class UGISWebWidget;

public:
    virtual void NativeConstruct() override;

    void SetupItem(FString InID, FString InName, FString InType, FString InParentID, FString InColor, float InOpacity, FString InTextColor, FString InTag, float InHeight, UGISWebWidget* InMainUI);

    void AddChildItem(UGISPolyItem* ChildWidget);
    void UpdateData(FString NewName, FString NewColor, float NewOpacity, FString NewTextColor, FString NewParentID);

    FString GetItemID() const 
    { 
        return ItemID; 
    }
    FString GetItemName() const 
    { 
        return ItemName; 
    }
    FString GetItemColor() const 
    { 
        return ItemColor; 
    }
    float GetItemOpacity() const 
    { 
        return ItemOpacity; 
    }
    FString GetItemTextColor() const 
    { 
        return ItemTextColor; 
    }
    FString GetItemParentID() const 
    { 
        return ItemParentID; 
    }
    FString GetItemType() const 
    { 
        return ItemType; 
    }

protected:
    UPROPERTY(meta = (BindWidget))
    UButton* Btn_Edit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* Txt_Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UTextBlock* Txt_Type;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UButton* Btn_Focus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UButton* Btn_Delete;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UVerticalBox* Child_Container;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (BindWidget))
    UBorder* Content_Border;

private:
    UFUNCTION()
    void OnFocusClicked();

    UFUNCTION()
    void OnDeleteClicked();

    UFUNCTION()
    void OnEditClicked();

    FString ItemID;
    FString ItemName;
    FString ItemType;
    FString ItemColor;
    float ItemOpacity;
    FString ItemTextColor;
    FString ItemTag;
    float ItemHeight;
    FString ItemParentID;

    TWeakObjectPtr<UGISWebWidget> MainUI;
};
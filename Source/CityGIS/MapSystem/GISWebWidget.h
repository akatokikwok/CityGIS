#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WebBrowserWidget/Public/WebBrowser.h"
#include "Components/ScrollBox.h"
#include "Components/EditableText.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/Border.h"
#include "GISPolyItem.h"
#include "GISLoadDialog.h"
#include "GISSaveDialog.h"
#include "GISWebWidget.generated.h"

USTRUCT(BlueprintType)
struct FGISSaveMetadata
{
    GENERATED_BODY()
    FString ID;
    FString Name;
    FString Description;
    FString Date;
    FString FilePath;
};

UCLASS()
class CITYGIS_API UGISWebWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;

    UFUNCTION(BlueprintCallable)
    void ActivateReconstructionTool();
    
    UFUNCTION(BlueprintCallable) 
    void SetMode(FString ModeName);

    UFUNCTION(BlueprintCallable) 
    void SaveMapAs(FString FileName);

    UFUNCTION(BlueprintCallable) 
    void LoadMap(FString FileName);

    UFUNCTION(BlueprintCallable) 
    TArray<FString> GetSaveFiles();
    
    void FocusID(FString ID);
    void DeleteID(FString ID);
    void FilterByType(FString TypeName);

    // 【新增】高亮列表项
    void HighlightListUI(FString ID);

    void OpenEditDialog(class UGISPolyItem* ItemToEdit);
    
    UFUNCTION(BlueprintCallable) 
    void SaveEditChanges();

    UFUNCTION(BlueprintCallable) 
    void CloseEditDialog();

    UFUNCTION(BlueprintCallable) 
    void OnPresetColorClicked(FLinearColor NewColor);

    UFUNCTION(BlueprintCallable) 
    void RequestSaveDataFromWeb();

    UFUNCTION(BlueprintCallable) 
    void OpenLoadDialog();
    
    void ExecuteSaveToFile(FString GeoJsonData, FString SaveName, FString SaveDesc);
    void ExecuteLoadFromFile(FString FilePath);

protected:
    UPROPERTY(meta = (BindWidget)) UWebBrowser* MapBrowser;
    UPROPERTY(meta = (BindWidget)) UScrollBox* List_Admin;
    UPROPERTY(meta = (BindWidget)) UScrollBox* List_Reconstruct;
    UPROPERTY(meta = (BindWidget)) UScrollBox* List_Road; 

    UPROPERTY(meta = (BindWidget)) UEditableText* Input_SaveName;
    UPROPERTY(meta = (BindWidget)) UComboBoxString* Combo_Files;
    UPROPERTY(meta = (BindWidget)) UComboBoxString* Combo_Filter;

    // 编辑弹窗组件
    UPROPERTY(meta = (BindWidget)) UWidget* Edit_Dialog_Overlay;
    UPROPERTY(meta = (BindWidget)) UEditableText* Edit_Input_Name;
    UPROPERTY(meta = (BindWidget)) UEditableText* Edit_Input_Color;
    UPROPERTY(meta = (BindWidget)) UEditableText* Edit_Input_Opacity;
    UPROPERTY(meta = (BindWidget)) UEditableText* Edit_Input_TextColor;
    
    UPROPERTY(meta = (BindWidget)) UComboBoxString* Edit_Input_Parent;

    UPROPERTY(meta = (BindWidget)) UBorder* Color_Preview;
    UPROPERTY(meta = (BindWidget)) UBorder* TextColor_Preview;
    
    UPROPERTY(meta = (BindWidget)) USlider* Slider_R;
    UPROPERTY(meta = (BindWidget)) USlider* Slider_G;
    UPROPERTY(meta = (BindWidget)) USlider* Slider_B;
    UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_R;
    UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_G;
    UPROPERTY(meta = (BindWidget)) USlider* Slider_Text_B;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<UGISPolyItem> PolyItemClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<UGISSaveDialog> SaveDialogClass;

    UPROPERTY(EditAnywhere, Category = "Config")
    TSubclassOf<UGISLoadDialog> LoadDialogClass;

private:
    UFUNCTION() 
    void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line);

    UFUNCTION() 
    void OnColorSliderChanged(float Value);

    UFUNCTION() 
    void OnTextColorSliderChanged(float Value);

    void ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color, float Opacity, FString TextColor, FString Tag, float Height);
    void UpdateColorUI(FLinearColor Color);
    void UpdateTextColorUI(FLinearColor Color);

   
    UPROPERTY() TMap<FString, UGISPolyItem*> WidgetMap = {};
    
    UPROPERTY() TWeakObjectPtr<UGISPolyItem> CurrentEditingItem;
    
    FString LastProcessedID;
    double LastLogTime = 0.0f;
};
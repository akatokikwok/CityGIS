#include "GISLoadDialog.h"
#include "GISWebWidget.h"
#include "GISFileItem.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

void UGISLoadDialog::NativeConstruct()
{
    Super::NativeConstruct();
    if (Btn_Load) Btn_Load->OnClicked.AddDynamic(this, &UGISLoadDialog::OnLoadClicked);
    if (Btn_Cancel) Btn_Cancel->OnClicked.AddDynamic(this, &UGISLoadDialog::OnCancelClicked);
    if (Btn_Delete) Btn_Delete->OnClicked.AddDynamic(this, &UGISLoadDialog::OnDeleteClicked);
}

void UGISLoadDialog::Init(UGISWebWidget* Parent)
{
    MainUI = Parent;
    AddToViewport(100);
    RefreshList();
}

void UGISLoadDialog::RefreshList()
{
    if (!FileList || !FileItemClass) return;
    FileList->ClearChildren();
    SelectedItem = nullptr;

    FString FolderPath = FPaths::ProjectSavedDir() + TEXT("GISData/");
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FolderPath, TEXT("*.json"));

    TArray<FGISSaveMetadata> MetaList;

    // 1. 读取所有文件信息
    for (const FString& FileName : Files)
    {
        FString FullPath = FolderPath + FileName;
        FString Content;
        if (FFileHelper::LoadFileToString(Content, *FullPath))
        {
            TSharedPtr<FJsonObject> JsonObj;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (FJsonSerializer::Deserialize(Reader, JsonObj))
            {
                FGISSaveMetadata Meta;
                Meta.Name = JsonObj->GetStringField("name");
                Meta.Date = JsonObj->GetStringField("date");
                Meta.Description = JsonObj->GetStringField("desc");
                Meta.FilePath = FullPath;
                MetaList.Add(Meta);
            }
        }
    }

    // 2. 排序 (最新在上)
    MetaList.Sort([](const FGISSaveMetadata& A, const FGISSaveMetadata& B) {
        return A.Date > B.Date;
    });

    // 3. 生成列表 UI
    for (const auto& Meta : MetaList)
    {
        UGISFileItem* Item = CreateWidget<UGISFileItem>(this, FileItemClass);
        if (Item)
        {
            Item->Setup(Meta, this);
            FileList->AddChild(Item);
            
            // 默认选中第一个
            if (!SelectedItem)
            {
                OnItemSelected(Item);
            }
        }
    }
}

void UGISLoadDialog::OnItemSelected(UGISFileItem* Item)
{
    if (SelectedItem) SelectedItem->SetSelected(false);
    SelectedItem = Item;
    if (SelectedItem) SelectedItem->SetSelected(true);
}

void UGISLoadDialog::OnLoadClicked()
{
    if (MainUI && SelectedItem)
    {
        MainUI->ExecuteLoadFromFile(SelectedItem->GetFilePath());
        RemoveFromParent();
    }
}

void UGISLoadDialog::OnDeleteClicked()
{
    if (SelectedItem)
    {
        IFileManager::Get().Delete(*SelectedItem->GetFilePath());
        RefreshList(); // 刷新列表
    }
}

void UGISLoadDialog::OnCancelClicked()
{
    RemoveFromParent();
}
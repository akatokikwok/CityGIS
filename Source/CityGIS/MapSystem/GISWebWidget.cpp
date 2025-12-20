#include "GISWebWidget.h"

#include "GISPolyItem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WebBrowserWidget/Public/WebBrowser.h"

void UGISWebWidget::RequestSaveDataFromWeb()
{
	if (MapBrowser)
	{
		// 通知 JS 把数据打包发回来
		MapBrowser->ExecuteJavascript(TEXT("requestAllDataForSave();"));
	}
}

void UGISWebWidget::OpenLoadDialog()
{
	if (LoadDialogClass)
	{
		UGISLoadDialog* Dialog = CreateWidget<UGISLoadDialog>(this, LoadDialogClass);
		if (Dialog)
		{
			Dialog->Init(this);
		}
	}
}

void UGISWebWidget::ExecuteSaveToFile(FString GeoJsonData, FString SaveName, FString SaveDesc)
{
	// 1. 构建元数据
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
    
	FString NewGuid = FGuid::NewGuid().ToString();
	FString NowTime = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	RootObject->SetStringField("id", NewGuid);
	RootObject->SetStringField("name", SaveName);
	RootObject->SetStringField("desc", SaveDesc);
	RootObject->SetStringField("date", NowTime);
    
	// 2. 将 GeoJsonData (字符串) 解析为对象放入，或者直接作为字符串存?
	// 为了方便，我们这里存为内嵌的 Object
	TSharedPtr<FJsonValue> GeoJsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GeoJsonData);
	if (FJsonSerializer::Deserialize(Reader, GeoJsonValue))
	{
		RootObject->SetField("data", GeoJsonValue);
	}
	else
	{
		// 如果解析失败，直接存字符串 (容错)
		RootObject->SetStringField("raw_data", GeoJsonData);
	}

	// 3. 序列化为最终 JSON 字符串
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	// 4. 写入文件: Saved/GISData/{GUID}.json
	FString FolderPath = FPaths::ProjectSavedDir() + TEXT("GISData/");
	IFileManager::Get().MakeDirectory(*FolderPath, true);
    
	// 使用时间戳+GUID做文件名，方便排序
	FString FileName = FString::Printf(TEXT("Save_%s_%s.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *NewGuid);
	FString FullPath = FolderPath + FileName;

	FFileHelper::SaveStringToFile(OutputString, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    
	UE_LOG(LogTemp, Log, TEXT("Saved to: %s"), *FullPath);
}

void UGISWebWidget::ExecuteLoadFromFile(FString FilePath)
{
	FString FileContent;
	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
        
		if (FJsonSerializer::Deserialize(Reader, JsonObj))
		{
			FString MapDataStr;
            
			if (JsonObj->HasField("data"))
			{
				const TSharedPtr<FJsonValue>& DataVal = JsonObj->GetField<EJson::None>("data");
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MapDataStr);
				FJsonSerializer::Serialize(DataVal, "", Writer);
			}
			else if (JsonObj->HasField("raw_data"))
			{
				MapDataStr = JsonObj->GetStringField("raw_data");
			}

			// 清理 UI
			if (List_Admin) List_Admin->ClearChildren();
			if (List_Reconstruct) List_Reconstruct->ClearChildren();
			WidgetMap.Empty();
			LastProcessedID = "";

			// 传给 JS
			MapDataStr = MapDataStr.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
			if (MapBrowser)
			{
				// 注意：数据量极大时这里可能需要拆分发送，目前假设数据量可控
				FString Script = TEXT("importMap('") + MapDataStr + TEXT("');");
				MapBrowser->ExecuteJavascript(Script);
			}
		}
	}
}

TArray<FGISSaveMetadata> UGISWebWidget::GetAllSaveFiles()
{
	TArray<FGISSaveMetadata> Result;
	FString FolderPath = FPaths::ProjectSavedDir() + TEXT("GISData/");
    
	// 查找所有 .json 文件
	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *FolderPath, TEXT("*.json"));

	for (const FString& FileName : FoundFiles)
	{
		FString FullPath = FolderPath + FileName;
		FString FileContent;
		if (FFileHelper::LoadFileToString(FileContent, *FullPath))
		{
			TSharedPtr<FJsonObject> JsonObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
			if (FJsonSerializer::Deserialize(Reader, JsonObj))
			{
				FGISSaveMetadata Meta;
				Meta.ID = JsonObj->GetStringField("id");
				Meta.Name = JsonObj->GetStringField("name");
				Meta.Description = JsonObj->GetStringField("desc");
				Meta.Date = JsonObj->GetStringField("date");
				Meta.FilePath = FullPath;
				Result.Add(Meta);
			}
		}
	}

	// 按日期倒序排序 (最新的在前面)
	Result.Sort([](const FGISSaveMetadata& A, const FGISSaveMetadata& B)
	{
		return A.Date > B.Date; 
	});

	return Result;
}

void UGISWebWidget::LoadSaveFile(FString FilePath)
{
	FString FileContent;
	if (FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(FileContent);
        
		if (FJsonSerializer::Deserialize(Reader, JsonObj))
		{
			// 提取核心数据部分
			FString MapDataStr;
            
			// 检查是用 'data' 对象存的，还是 'raw_data' 字符串存的
			if (JsonObj->HasField("data"))
			{
				const TSharedPtr<FJsonValue>& DataVal = JsonObj->GetField<EJson::None>("data");
                
				// 将 data 对象反序列化回字符串传给 JS
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MapDataStr);
				FJsonSerializer::Serialize(DataVal, "", Writer);
			}
			else if (JsonObj->HasField("raw_data"))
			{
				MapDataStr = JsonObj->GetStringField("raw_data");
			}

			// 清理 C++ 列表 UI
			if (List_Admin) List_Admin->ClearChildren();
			if (List_Reconstruct) List_Reconstruct->ClearChildren();
			WidgetMap.Empty();
			LastProcessedID = "";

			// 清理换行符，防止 JS 报错
			MapDataStr = MapDataStr.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));

			// 调用 JS 加载
			if (MapBrowser)
			{
				// 使用 FString::Printf 可能会因为 JSON 太长而崩溃，建议用 ExecuteJavascript
				// 但为了安全，这里我们拼接字符串
				FString Script = TEXT("importMap('") + MapDataStr + TEXT("');");
				MapBrowser->ExecuteJavascript(Script);
			}
		}
	}
}

void UGISWebWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 设置存档路径: Saved/GISData/Data.json
	SaveFilePath = FPaths::ProjectSavedDir() + TEXT("GISData/Data.json");

	// 2. 加载本地网页
	if (MapBrowser)
	{
		FString HtmlPath = FPaths::ProjectContentDir() + TEXT("HTML/map_engine.html");
		FString Url = TEXT("file://") + FPaths::ConvertRelativePathToFull(HtmlPath);
		MapBrowser->LoadURL(Url);

		// 绑定通信
		MapBrowser->OnUrlChanged.AddUniqueDynamic(this, &UGISWebWidget::OnTitleChanged);

		MapBrowser->OnConsoleMessage.AddUniqueDynamic(this, &UGISWebWidget::HandleConsoleMessage);
	}

	// 绑定滑块事件
	if (Slider_R) Slider_R->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
	if (Slider_G) Slider_G->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
	if (Slider_B) Slider_B->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
}

void UGISWebWidget::NativeDestruct()
{
	Super::NativeDestruct();

	if (MapBrowser)
	{
		MapBrowser->OnUrlChanged.RemoveDynamic(this, &UGISWebWidget::OnTitleChanged);
		MapBrowser->OnConsoleMessage.RemoveDynamic(this, &UGISWebWidget::HandleConsoleMessage);
	}
}

void UGISWebWidget::SetMode(FString ModeName)
{
	if (MapBrowser)
	{
		// 调用 JS 函数: setMode('...')
		FString Script = FString::Printf(TEXT("setMode('%s');"), *ModeName);
		MapBrowser->ExecuteJavascript(Script);
	}
}

void UGISWebWidget::UndoAction()
{
	if (MapBrowser) MapBrowser->ExecuteJavascript(TEXT("undoLastPoint();"));
}

void UGISWebWidget::SaveMapAs(FString FileName)
{
	if (!FileName.EndsWith(".json")) FileName += ".json";
	if (MapBrowser)
	{
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("exportMap('%s');"), *FileName));
	}
}

void UGISWebWidget::OnTitleChanged(const FText& TitleText)
{
}

void UGISWebWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	if (!Message.StartsWith("UE_")) return;

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastLogTime < 0.02) return;
	LastLogTime = CurrentTime;

	// UE_ADD:ID|Name|Type|ParentID|Color|Opacity|TextColor|Tag|Time
	if (Message.StartsWith("UE_ADD:"))
	{
		FString Content = Message.RightChop(7);
		TArray<FString> Parts;
		Content.ParseIntoArray(Parts, TEXT("|"), false);

		// 1. 确保参数数量为 10
		if (Parts.Num() >= 10)
		{
			FString ID = Parts[0];
			FString Name = Parts[1];
			FString Type = Parts[2];
			FString ParentID = Parts[3];
			FString Color = Parts[4];
			float Opacity = FCString::Atof(*Parts[5]);
			FString TextColor = Parts[6];
			FString Tag = Parts[7]; // 获取 Tag
			// 【新增】解析高度字段
			float Height = FCString::Atof(*Parts[8]);
			
			if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase)) return;
			LastProcessedID = ID;

			// 2. 调用构建
			// 【修改】调用时传入 Height
			ProcessAddPolyItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag, Height);

			// 3. OnAddPolyItemDelegate
			if (OnAddPolyItemDelegate.IsBound())
			{
				// 广播三个参数
				OnAddPolyItemDelegate.Broadcast(ID, Name, Type, ParentID);
			}
		}
	}
	// 2. 【新增】接收 Web 发来的完整地图数据 (准备存档)
	else if (Message.StartsWith("UE_EXPORT_DATA:"))
	{
		FString RawJson = Message.RightChop(15);
        
		// 创建纯 C++ 控制的保存弹窗
		if (SaveDialogClass)
		{
			UGISSaveDialog* Dialog = CreateWidget<UGISSaveDialog>(this, SaveDialogClass);
			if (Dialog)
			{
				Dialog->Init(this, RawJson);
			}
		}
	}
}

// 核心：C++ 构建树状 UI
void UGISWebWidget::ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color, float Opacity, FString TextColor, FString Tag, float Height)
{
	if (!PolyItemClass)
	{
		return;
	}

	UGISPolyItem* NewItem = CreateWidget<UGISPolyItem>(this, PolyItemClass);
	if (!NewItem)
	{
		return;
	}

	// 【修改】传入 Height
	NewItem->SetupItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag, Height, this);
	WidgetMap.Add(ID, NewItem);

	// 层级挂载逻辑
	if (Type == "Reconstruct") 
	{
		if (List_Reconstruct)
		{
			List_Reconstruct->AddChild(NewItem);
		}
	}
	else if (Type == "District") 
	{
		if (List_Admin)
		{
			List_Admin->AddChild(NewItem);
		}
	}
	else if (Type == "Road") 
	{
		// 【新增】道路类型挂载到 List_Road
		if (List_Road)
		{
			List_Road->AddChild(NewItem);
		}
	}
	// 【修改】Road 和 Custom 一样，都是子级元素，尝试挂载到 ParentID (街道) 下
	else if (Type == "Street" || Type == "Community" || Type == "Custom" || Type == "Road") 
	{
		UGISPolyItem** ParentWidgetPtr = WidgetMap.Find(ParentID);
        
		if (ParentWidgetPtr && *ParentWidgetPtr) 
		{
			(*ParentWidgetPtr)->AddChildItem(NewItem);
		}
		else if (List_Admin) 
		{
			// 如果找不到父级，挂到根目录防止数据丢失
			List_Admin->AddChild(NewItem);
		}
	}
}

void UGISWebWidget::OnColorSliderChanged(float Value)
{
	if (Slider_R && Slider_G && Slider_B)
	{
		FLinearColor NewColor(Slider_R->GetValue(), Slider_G->GetValue(), Slider_B->GetValue());

		// 更新预览和 Hex 文本
		if (Color_Preview) Color_Preview->SetBrushColor(NewColor);
		if (Edit_Input_Color) Edit_Input_Color->SetText(FText::FromString("#" + NewColor.ToFColor(true).ToHex()));
	}
}

void UGISWebWidget::UpdateColorUI(FLinearColor Color)
{
	// 1. 更新滑块位置
	if (Slider_R) Slider_R->SetValue(Color.R);
	if (Slider_G) Slider_G->SetValue(Color.G);
	if (Slider_B) Slider_B->SetValue(Color.B);

	// 2. 更新预览色块
	if (Color_Preview) Color_Preview->SetBrushColor(Color);

	// 3. 更新 Hex 文本框
	if (Edit_Input_Color) Edit_Input_Color->SetText(FText::FromString("#" + Color.ToFColor(true).ToHex()));
}

void UGISWebWidget::UpdateTextColorUI(FLinearColor Color)
{
	if (Slider_Text_R) Slider_Text_R->SetValue(Color.R);
	if (Slider_Text_G) Slider_Text_G->SetValue(Color.G);
	if (Slider_Text_B) Slider_Text_B->SetValue(Color.B);
	if (TextColor_Preview) TextColor_Preview->SetBrushColor(Color);
	if (Edit_Input_TextColor) Edit_Input_TextColor->SetText(FText::FromString("#" + Color.ToFColor(true).ToHex()));
}

void UGISWebWidget::LoadMap(FString FileName)
{
	// 载入前清空 UI 和 字典
	if (List_Admin) List_Admin->ClearChildren();
	if (List_Reconstruct) List_Reconstruct->ClearChildren();
	WidgetMap.Empty();
	LastProcessedID = "";

	FString Path = FPaths::ProjectSavedDir() + TEXT("GISData/") + FileName;
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *Path))
	{
		Json = Json.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
		if (MapBrowser) MapBrowser->ExecuteJavascript(FString::Printf(TEXT("importMap('%s');"), *Json));
	}
}

TArray<FString> UGISWebWidget::GetSaveFiles()
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(FPaths::ProjectSavedDir() + TEXT("GISData/*.json")), true, false);
	return Files;
}

void UGISWebWidget::FocusID(FString ID)
{
	MapBrowser->ExecuteJavascript(FString::Printf(TEXT("focusPoly('%s');"), *ID));
}

void UGISWebWidget::DeleteID(FString ID)
{
	if (MapBrowser) MapBrowser->ExecuteJavascript(FString::Printf(TEXT("deletePoly('%s');"), *ID));
	// C++ 侧同时也清理字典和UI
	if (UGISPolyItem** Item = WidgetMap.Find(ID))
	{
		if (*Item) (*Item)->RemoveFromParent();
		WidgetMap.Remove(ID);
	}
}

void UGISWebWidget::SearchByName(FString Name)
{
	if (MapBrowser)
	{
		FString Script = FString::Printf(TEXT("searchPoly('%s');"), *Name);
		MapBrowser->ExecuteJavascript(Script);
	}
}

void UGISWebWidget::ActivateReconstructionTool()
{
	if (MapBrowser)
	{
		// 切换到 JS 的 'reconstruct' 模式
		MapBrowser->ExecuteJavascript(TEXT("setMode('reconstruct');"));
	}
}

void UGISWebWidget::FilterByType(FString TypeName)
{
	if (MapBrowser)
	{
		// 调用 JS: filterPolys('Normal')
		FString Script = FString::Printf(TEXT("filterPolys('%s');"), *TypeName);
		MapBrowser->ExecuteJavascript(Script);
	}
}

void UGISWebWidget::OpenEditDialog(class UGISPolyItem* ItemToEdit)
{
	if (!ItemToEdit) return;
	CurrentEditingItem = ItemToEdit;
	if (Edit_Input_Name) Edit_Input_Name->SetText(FText::FromString(ItemToEdit->GetItemName()));
	if (Edit_Input_Opacity) Edit_Input_Opacity->SetText(FText::AsNumber(ItemToEdit->GetItemOpacity()));

	FColor FillColor = FColor::FromHex(ItemToEdit->GetItemColor());
	UpdateColorUI(FLinearColor::FromSRGBColor(FillColor));
	FColor TextCol = FColor::FromHex(ItemToEdit->GetItemTextColor());
	UpdateTextColorUI(FLinearColor::FromSRGBColor(TextCol));

	if (Edit_Dialog_Overlay) Edit_Dialog_Overlay->SetVisibility(ESlateVisibility::Visible);
}

void UGISWebWidget::SaveEditChanges()
{
	// 确保当前有选中的项，且浏览器有效
	if (CurrentEditingItem.IsValid() && MapBrowser)
	{
		FString NewName = Edit_Input_Name->GetText().ToString();
		FString NewColor = Edit_Input_Color->GetText().ToString();
		FString NewOpacityStr = Edit_Input_Opacity->GetText().ToString();
		float NewOpacity = FCString::Atof(*NewOpacityStr);
		FString NewTextColor = Edit_Input_TextColor->GetText().ToString();
		FString ID = CurrentEditingItem->GetItemID();

		FString Script = FString::Printf(
			TEXT("updatePolyAttributes('%s', '%s', '%s', '%s', '%s');"), *ID, *NewName, *NewColor, *NewOpacityStr,
			*NewTextColor);
		MapBrowser->ExecuteJavascript(Script);
		CurrentEditingItem->UpdateData(NewName, NewColor, NewOpacity, NewTextColor);
	}
	CloseEditDialog();
}

void UGISWebWidget::CloseEditDialog()
{
	if (Edit_Dialog_Overlay) Edit_Dialog_Overlay->SetVisibility(ESlateVisibility::Collapsed);
	CurrentEditingItem = nullptr;
}

void UGISWebWidget::OnPresetColorClicked(FLinearColor NewColor)
{
	UpdateColorUI(NewColor);
}

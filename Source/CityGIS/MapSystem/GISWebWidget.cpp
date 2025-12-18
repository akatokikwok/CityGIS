#include "GISWebWidget.h"

#include "GISPolyItem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WebBrowserWidget/Public/WebBrowser.h"

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
	// // Message 就是 JS 里 console.log("...") 的内容
	//
	// // 为了防止处理无关的报错日志，我们只处理以 UE_ 开头的
	// if (!Message.StartsWith("UE_")) return;
	//
	// // 【核心修复 2】微秒级时间防抖 (Time Debounce)
	// // 如果这条消息距离上一条消息小于 0.1 秒，直接视为引擎回显或BUG，丢弃。
	// // (因为我们的 JS 队列是间隔 0.3 秒发送的，所以合法消息不会被误杀)
	// double CurrentTime = FPlatformTime::Seconds();
	// const double& DeltaMinus = CurrentTime - LastLogTime;
	// if (DeltaMinus < 0.01)
	// {
	// 	return;
	// }
	// LastLogTime = CurrentTime;
	//
	// // 打印一下，确保我们收到了
	// UE_LOG(LogTemp, Log, TEXT("GIS Received JS Message: %s"), *Message);
	//
	// // 1. 处理保存: UE_SAVE:文件名|JSON
	// if (Message.StartsWith("UE_SAVE:"))
	// {
	// 	FString Content = Message.RightChop(8);
	// 	FString Name, Data;
	// 	if (Content.Split("|", &Name, &Data))
	// 	{
	// 		FString Path = FPaths::ProjectSavedDir() + TEXT("GISData/") + Name;
	// 		FFileHelper::SaveStringToFile(Data, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	// 		UE_LOG(LogTemp, Warning, TEXT("GIS: Saved File to %s"), *Path);
	// 	}
	// }
	// // 2. 处理添加列表项: UE_ADD:ID|Name
	// else if (Message.StartsWith("UE_ADD:"))
	// {
	// 	FString Content = Message.RightChop(7);
	// 	// FString ID, Name, Type, ParentID;
	//
	// 	// 格式: ID|Name|Type|Timestamp
	// 	// 我们需要拆解前三个
	// 	TArray<FString> Parts;
	// 	Content.ParseIntoArray(Parts, TEXT("|"), false);
	//
	// 	// 【关键】现在有 9 个部分
	// 	if (Parts.Num() >= 9) 
	// 	{
	// 		FString ID = Parts[0];
	// 		FString Name = Parts[1];
	// 		FString Type = Parts[2];
	// 		FString ParentID = Parts[3];
	// 		FString Color = Parts[4];
	// 		float Opacity = FCString::Atof(*Parts[5]);
	// 		FString TextColor = Parts[6];
	// 		FString Tag = Parts[7]; // 新增 Tag
	//
	// 		// ID 查重 (保持不变)
	// 		if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase)) return;
	// 		LastProcessedID = ID;
	//
	// 		// 调用 C++ 内部函数构建 UI
	// 		// 调用处理函数 (你需要修改这个函数的签名)
	// 		ProcessAddPolyItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag);
	//
	// 		if (OnAddPolyItemDelegate.IsBound())
	// 		{
	// 			// 广播三个参数
	// 			OnAddPolyItemDelegate.Broadcast(ID, Name, Type, ParentID);
	// 		}
	// 	}
	// }


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

		// 1. 确保参数数量为 9
		if (Parts.Num() >= 9)
		{
			FString ID = Parts[0];
			FString Name = Parts[1];
			FString Type = Parts[2];
			FString ParentID = Parts[3];
			FString Color = Parts[4];
			float Opacity = FCString::Atof(*Parts[5]);
			FString TextColor = Parts[6];
			FString Tag = Parts[7]; // 获取 Tag

			if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase)) return;
			LastProcessedID = ID;

			// 2. 调用构建
			ProcessAddPolyItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag);

			// 3. OnAddPolyItemDelegate
			if (OnAddPolyItemDelegate.IsBound())
			{
				// 广播三个参数
				OnAddPolyItemDelegate.Broadcast(ID, Name, Type, ParentID);
			}
		}
	}
	else if (Message.StartsWith("UE_SAVE:"))
	{
		FString Content = Message.RightChop(8);
		FString Name, Data;
		if (Content.Split("|", &Name, &Data))
		{
			FString Path = FPaths::ProjectSavedDir() + TEXT("GISData/") + Name;
			FFileHelper::SaveStringToFile(Data, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}
	}
}

// 核心：C++ 构建树状 UI
void UGISWebWidget::ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color,
                                       float Opacity, FString TextColor, FString Tag)
{
	if (!PolyItemClass) return;

	UGISPolyItem* NewItem = CreateWidget<UGISPolyItem>(this, PolyItemClass);
	if (!NewItem) return;

	NewItem->SetupItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag, this);
	WidgetMap.Add(ID, NewItem);

	// 3. 挂载逻辑更新
	if (Type == "Reconstruct")
	{
		if (List_Reconstruct) List_Reconstruct->AddChild(NewItem);
	}
	else if (Type == "District")
	{
		if (List_Admin) List_Admin->AddChild(NewItem);
	}
	// Custom 与 Community 一样，尝试挂载到 ParentID (街道) 下
	else if (Type == "Street" || Type == "Community" || Type == "Custom")
	{
		UGISPolyItem** ParentWidgetPtr = WidgetMap.Find(ParentID);
		if (ParentWidgetPtr && *ParentWidgetPtr)
		{
			(*ParentWidgetPtr)->AddChildItem(NewItem);
		}
		else if (List_Admin)
		{
			List_Admin->AddChild(NewItem); // 找不到爹就挂根目录
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

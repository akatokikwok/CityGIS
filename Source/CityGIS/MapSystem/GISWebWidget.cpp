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
	// Message 就是 JS 里 console.log("...") 的内容

	// 为了防止处理无关的报错日志，我们只处理以 UE_ 开头的
	if (!Message.StartsWith("UE_")) return;

	// 【核心修复 2】微秒级时间防抖 (Time Debounce)
	// 如果这条消息距离上一条消息小于 0.1 秒，直接视为引擎回显或BUG，丢弃。
	// (因为我们的 JS 队列是间隔 0.3 秒发送的，所以合法消息不会被误杀)
	double CurrentTime = FPlatformTime::Seconds();
	const double& DeltaMinus = CurrentTime - LastLogTime;
	if (DeltaMinus < 0.01)
	{
		return;
	}
	LastLogTime = CurrentTime;

	// 打印一下，确保我们收到了
	UE_LOG(LogTemp, Log, TEXT("GIS Received JS Message: %s"), *Message);

	// 1. 处理保存: UE_SAVE:文件名|JSON
	if (Message.StartsWith("UE_SAVE:"))
	{
		FString Content = Message.RightChop(8);
		FString Name, Data;
		if (Content.Split("|", &Name, &Data))
		{
			FString Path = FPaths::ProjectSavedDir() + TEXT("GISData/") + Name;
			FFileHelper::SaveStringToFile(Data, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogTemp, Warning, TEXT("GIS: Saved File to %s"), *Path);
		}
	}
	// 2. 处理添加列表项: UE_ADD:ID|Name
	else if (Message.StartsWith("UE_ADD:"))
	{
		FString Content = Message.RightChop(7);
		// FString ID, Name, Type, ParentID;

		// 格式: ID|Name|Type|Timestamp
		// 我们需要拆解前三个
		TArray<FString> Parts;
		Content.ParseIntoArray(Parts, TEXT("|"), false);

		if (Parts.Num() >= 7) // 至少要有7个部分
		{
			FString ID = Parts[0];
			FString Name = Parts[1];
			FString Type = Parts[2];
			FString ParentID = Parts[3];
			FString Color = Parts[4];
			float Opacity = FCString::Atof(*Parts[5]);

			// ID 查重 (保持不变)
			if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase)) return;
			LastProcessedID = ID;

			// 调用 C++ 内部函数构建 UI
			ProcessAddPolyItem(ID, Name, Type, ParentID, Color, Opacity);

			if (OnAddPolyItemDelegate.IsBound())
			{
				// 广播三个参数
				OnAddPolyItemDelegate.Broadcast(ID, Name, Type, ParentID);
			}
		}
	}
}

// 核心：C++ 构建树状 UI
void UGISWebWidget::ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color,
                                       float Opacity) // 更新参数
{
	if (!PolyItemClass)
	{
		UE_LOG(LogTemp, Error, TEXT("GIS: PolyItemClass not set in BP_GISMain!"));
		return;
	}

	// 1. 创建 Widget
	UGISPolyItem* NewItem = CreateWidget<UGISPolyItem>(this, PolyItemClass);
	if (!NewItem) return;

	// 2. 初始化数据
	// 传入新参数
	NewItem->SetupItem(ID, Name, Type, ParentID, Color, Opacity, this);

	// 3. 存入字典
	WidgetMap.Add(ID, NewItem);

	// 4. 挂载到 UI
	if (Type == "Reconstruct")
	{
		// 重构块 -> 单独列表
		if (List_Reconstruct) List_Reconstruct->AddChild(NewItem);
	}
	else if (Type == "District")
	{
		// 区镇 -> 顶级列表
		if (List_Admin) List_Admin->AddChild(NewItem);
	}
	else if (Type == "Street" || Type == "Community")
	{
		// 街道/小区 -> 找爸爸
		UGISPolyItem** ParentWidgetPtr = WidgetMap.Find(ParentID);
		if (ParentWidgetPtr && *ParentWidgetPtr)
		{
			// 找到了爸爸，加到爸爸怀里 (Child_Container)
			(*ParentWidgetPtr)->AddChildItem(NewItem);
		}
		else
		{
			// 没找到爸爸 (可能是孤儿数据)，做容错处理，加到最外层
			UE_LOG(LogTemp, Warning, TEXT("GIS: Orphan Item %s (Parent %s not found)"), *Name, *ParentID);
			if (List_Admin) List_Admin->AddChild(NewItem);
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

	// 填充名字
	if (Edit_Input_Name) Edit_Input_Name->SetText(FText::FromString(ItemToEdit->GetItemName()));
	if (Edit_Input_Opacity) Edit_Input_Opacity->SetText(FText::AsNumber(ItemToEdit->GetItemOpacity()));

	// 解析颜色 Hex -> LinearColor
	FString Hex = ItemToEdit->GetItemColor();
	FLinearColor CurrentColor = FLinearColor::Blue; // 默认防崩
	FColor SrgbColor = FColor::FromHex(Hex);
	CurrentColor = FLinearColor::FromSRGBColor(SrgbColor);

	// 更新 UI
	UpdateColorUI(CurrentColor);

	if (Edit_Dialog_Overlay) Edit_Dialog_Overlay->SetVisibility(ESlateVisibility::Visible);
}

void UGISWebWidget::SaveEditChanges()
{
	// 确保当前有选中的项，且浏览器有效
	if (CurrentEditingItem.IsValid() && MapBrowser)
	{
		// 直接从 Hex 文本框取值 (因为它被滑块实时更新了)
		FString NewName = Edit_Input_Name->GetText().ToString();
		FString NewColor = Edit_Input_Color->GetText().ToString();
		FString NewOpacityStr = Edit_Input_Opacity->GetText().ToString();
		float NewOpacity = FCString::Atof(*NewOpacityStr); // 转为 float
        
		FString ID = CurrentEditingItem->GetItemID();

		// 1. 调用 JS 更新地图 (保持不变)
		FString Script = FString::Printf(TEXT("updatePolyAttributes('%s', '%s', '%s', '%s');"), *ID, *NewName, *NewColor, *NewOpacityStr);
		MapBrowser->ExecuteJavascript(Script);

		// 2. 【核心修复】调用列表项的自我更新函数
		CurrentEditingItem->UpdateData(NewName, NewColor, NewOpacity);
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

#include "GISWebWidget.h"
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
	// 让 JS 导出数据，带上文件名
	MapBrowser->ExecuteJavascript(FString::Printf(TEXT("exportMap('%s');"), *FileName));
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
		FString ID, Name;
        
		if (Content.Split("|", &ID, &Name))
		{
			// 【核心修复 3】ID 查重 (配合头文件变量)
			if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase))
			{
				UE_LOG(LogTemp, Warning, TEXT("GIS: Blocked Duplicate ID: %s"), *ID);
				return; // 拦截！
			}
			LastProcessedID = ID; // 更新记录

			// 清理可能存在的后缀
			FString RealName, Trash;
			if(Name.Split("|", &RealName, &Trash)) Name = RealName;

			if (OnAddPolyItemDelegate.IsBound())
			{
				OnAddPolyItemDelegate.Broadcast(ID, Name);
			}
		}
	}
}

void UGISWebWidget::LoadMap(FString FileName)
{
	FString Path = FPaths::ProjectSavedDir() + TEXT("GISData/") + FileName;
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *Path))
	{
		Json = Json.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("importMap('%s');"), *Json));
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
	MapBrowser->ExecuteJavascript(FString::Printf(TEXT("deletePoly('%s');"), *ID));
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

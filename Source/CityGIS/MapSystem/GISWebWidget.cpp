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
		MapBrowser->OnUrlChanged.AddDynamic(this, &UGISWebWidget::OnTitleChanged);
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

void UGISWebWidget::SaveMap()
{
	// 触发 JS 导出 -> JS 改 Title -> OnTitleChanged 捕获 -> 写文件
	if (MapBrowser) MapBrowser->ExecuteJavascript(TEXT("exportData();"));
}

void UGISWebWidget::OnTitleChanged(const FText& TitleText)
{
	FString TitleStr = TitleText.ToString();
	// 拦截协议头 "UE_SAVE:"
	if (TitleStr.StartsWith("UE_SAVE:"))
	{
		FString JsonData = TitleStr.RightChop(8); // 截取后面的 JSON
		FFileHelper::SaveStringToFile(JsonData, *SaveFilePath);
		UE_LOG(LogTemp, Warning, TEXT("Map Saved to: %s"), *SaveFilePath);
	}
}

void UGISWebWidget::LoadMap()
{
	FString JsonContent;
	if (FFileHelper::LoadFileToString(JsonContent, *SaveFilePath))
	{
		// 清理换行符
		JsonContent = JsonContent.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
		FString Script = FString::Printf(TEXT("importData('%s');"), *JsonContent);
		MapBrowser->ExecuteJavascript(Script);
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

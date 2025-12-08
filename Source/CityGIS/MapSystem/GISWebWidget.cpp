#include "GISWebWidget.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "WebBrowserWidget/Public/WebBrowser.h"

void UGISWebWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 1. 动态加载本地 HTML (处理路径问题)
	FString HtmlFile = FPaths::ProjectContentDir() + TEXT("HTML/map_engine.html");
	FString Url = TEXT("file://") + FPaths::ConvertRelativePathToFull(HtmlFile);

	if (MainBrowser)
	{
		MainBrowser->LoadURL(FString::Printf(TEXT("file:///E:/Work/Project/CityGIS/CityGIS/Content/HTML/map_engine.html")));
		// 绑定通信监听
		MainBrowser->OnUrlChanged.AddDynamic(this, &UGISWebWidget::OnTitleChanged);
	}

	SavePath = FPaths::ProjectSavedDir() + TEXT("GISData/Level_1.json");
}

void UGISWebWidget::ToggleMode(bool bIsEdit)
{
	if (!MainBrowser) return;
	FString Mode = bIsEdit ? TEXT("edit") : TEXT("browse");
	FString Script = FString::Printf(TEXT("setMode('%s');"), *Mode);
	MainBrowser->ExecuteJavascript(Script);
}

void UGISWebWidget::SaveData()
{
	// 发送指令给 JS，让 JS 整理数据
	if (MainBrowser) MainBrowser->ExecuteJavascript(TEXT("exportData();"));
}

void UGISWebWidget::OnTitleChanged(const FText& TitleText)
{
	FString TitleStr = TitleText.ToString();
	// 拦截特定协议头
	if (TitleStr.StartsWith(TEXT("UE_SAVE:")))
	{
		FString JsonContent = TitleStr.RightChop(8); // 去掉头
		FFileHelper::SaveStringToFile(JsonContent, *SavePath);
		UE_LOG(LogTemp, Log, TEXT("GIS Data Saved to: %s"), *SavePath);
	}
}

void UGISWebWidget::LoadData()
{
	FString JsonContent;
	if (FFileHelper::LoadFileToString(JsonContent, *SavePath))
	{
		// 清洗换行符，防止 JS 报错
		JsonContent = JsonContent.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
		FString Script = FString::Printf(TEXT("importData('%s');"), *JsonContent);
		MainBrowser->ExecuteJavascript(Script);
	}
}

void UGISWebWidget::AddMarkerAtCenter(FString Type)
{
	// 获取当前地图中心并标记 (需 JS 端配合 getCenter 接口，此处略，逻辑类似)
}

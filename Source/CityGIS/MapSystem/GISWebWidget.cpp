#include "GISWebWidget.h"
#include "GISPolyItem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "WebBrowserWidget/Public/WebBrowser.h"

void UGISWebWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MapBrowser)
	{
		FString HtmlPath = FPaths::ProjectContentDir() + TEXT("HTML/map_engine.html");
		FString Url = TEXT("file://") + FPaths::ConvertRelativePathToFull(HtmlPath);
		MapBrowser->LoadURL(Url);

		if (!MapBrowser->OnConsoleMessage.IsAlreadyBound(this, &UGISWebWidget::HandleConsoleMessage))
		{
			MapBrowser->OnConsoleMessage.AddUniqueDynamic(this, &UGISWebWidget::HandleConsoleMessage);
		}
	}

	if (Slider_R)
	{
		Slider_R->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
	}
	if (Slider_G)
	{
		Slider_G->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
	}
	if (Slider_B)
	{
		Slider_B->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnColorSliderChanged);
	}
	if (Slider_Text_R)
	{
		Slider_Text_R->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnTextColorSliderChanged);
	}
	if (Slider_Text_G)
	{
		Slider_Text_G->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnTextColorSliderChanged);
	}
	if (Slider_Text_B)
	{
		Slider_Text_B->OnValueChanged.AddUniqueDynamic(this, &UGISWebWidget::OnTextColorSliderChanged);
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

void UGISWebWidget::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line)
{
	if (!Message.StartsWith("UE_"))
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastLogTime < 0.02)
	{
		return;
	}
	LastLogTime = CurrentTime;

	if (Message.StartsWith("UE_ADD:"))
	{
		FString Content = Message.RightChop(7);
		TArray<FString> Parts;
		Content.ParseIntoArray(Parts, TEXT("|"), false);

		if (Parts.Num() >= 10)
		{
			FString ID = Parts[0];
			FString Name = Parts[1];
			FString Type = Parts[2];
			FString ParentID = Parts[3];
			FString Color = Parts[4];
			float Opacity = FCString::Atof(*Parts[5]);
			FString TextColor = Parts[6];
			FString Tag = Parts[7];
			float Height = FCString::Atof(*Parts[8]);

			if (ID.Equals(LastProcessedID, ESearchCase::IgnoreCase))
			{
				return;
			}
			LastProcessedID = ID;

			ProcessAddPolyItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag, Height);
		}
	}
	else if (Message.StartsWith("UE_EXPORT_DATA:"))
	{
		FString RawJson = Message.RightChop(15);
		if (SaveDialogClass)
		{
			UGISSaveDialog* Dialog = CreateWidget<UGISSaveDialog>(this, SaveDialogClass);
			if (Dialog)
			{
				Dialog->Init(this, RawJson);
			}
		}
	}
	// 【新增】处理双击高亮
	else if (Message.StartsWith("UE_DBLCLICK:"))
	{
		FString TargetID = Message.RightChop(12);
		HighlightListUI(TargetID);
	}
}

void UGISWebWidget::HighlightListUI(FString ID)
{
	// 先取消所有高亮 (简化处理)
	// 实际项目中可以在 GISPolyItem 里加 SetHighlight 函数改变颜色
	// 这里演示逻辑：找到该 Widget 并 Scroll 到它
	if (WidgetMap.Contains(ID))
	{
		UGISPolyItem* Item = WidgetMap[ID];
		if (Item)
		{
			if (List_Admin)
			{
				List_Admin->ScrollWidgetIntoView(Item);
			}
			// 这里可以扩展 Item->SetHighlight(true);
		}
	}
}

void UGISWebWidget::ProcessAddPolyItem(FString ID, FString Name, FString Type, FString ParentID, FString Color,
                                       float Opacity, FString TextColor, FString Tag, float Height)
{
	if (!PolyItemClass)
	{
		return;
	}

	// 【核心逻辑】自动构建父级
	// 如果是街道且没有父级，尝试根据 Tag (行政区代码) 创建父级
	if (Type == "Street" && (ParentID == "None" || ParentID.IsEmpty()) && !Tag.IsEmpty())
	{
		// 假设 Tag 是 310101 (黄浦区)
		FString DistrictID = "District_" + Tag; // 生成父级ID

		// 检查父级是否存在
		if (!WidgetMap.Contains(DistrictID))
		{
			// 创建父级 (虚拟的，没有几何信息)
			UGISPolyItem* ParentItem = CreateWidget<UGISPolyItem>(this, PolyItemClass);
			if (ParentItem)
			{
				// 父级名字暂时用 Tag 代替，实际可以查表
				FString DistrictName = FString::Printf(TEXT("行政区_%s"), *Tag);
				ParentItem->SetupItem(DistrictID, DistrictName, "District", "None", "#808080", 1.0f, "#FFFFFF", "", 0,
				                      this);
				WidgetMap.Add(DistrictID, ParentItem);

				if (List_Admin)
				{
					List_Admin->AddChild(ParentItem);
				}
			}
		}
		// 修正当前项的 ParentID
		ParentID = DistrictID;
	}

	UGISPolyItem* NewItem = CreateWidget<UGISPolyItem>(this, PolyItemClass);
	if (!NewItem)
	{
		return;
	}

	NewItem->SetupItem(ID, Name, Type, ParentID, Color, Opacity, TextColor, Tag, Height, this);
	WidgetMap.Add(ID, NewItem);

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
		if (List_Road)
		{
			List_Road->AddChild(NewItem);
		}
	}
	else if (Type == "Street" || Type == "Community" || Type == "Custom")
	{
		UGISPolyItem** ParentWidgetPtr = WidgetMap.Find(ParentID);
		if (ParentWidgetPtr && *ParentWidgetPtr)
		{
			(*ParentWidgetPtr)->AddChildItem(NewItem);
		}
		else if (List_Admin)
		{
			List_Admin->AddChild(NewItem);
		}
	}
}

void UGISWebWidget::OpenEditDialog(UGISPolyItem* ItemToEdit)
{
	if (!ItemToEdit)
	{
		return;
	}
	CurrentEditingItem = ItemToEdit;

	if (Edit_Input_Name)
	{
		Edit_Input_Name->SetText(FText::FromString(ItemToEdit->GetItemName()));
	}
	if (Edit_Input_Opacity)
	{
		Edit_Input_Opacity->SetText(FText::AsNumber(ItemToEdit->GetItemOpacity()));
	}

	FColor FillColor = FColor::FromHex(ItemToEdit->GetItemColor());
	UpdateColorUI(FLinearColor::FromSRGBColor(FillColor));
	FColor TextCol = FColor::FromHex(ItemToEdit->GetItemTextColor());
	UpdateTextColorUI(FLinearColor::FromSRGBColor(TextCol));

	if (Edit_Input_Parent)
	{
		Edit_Input_Parent->ClearOptions();
		Edit_Input_Parent->AddOption(TEXT("None (无)"));

		FString TargetParentType = "";
		FString CurrentType = ItemToEdit->GetItemType();

		if (CurrentType == "Street")
		{
			TargetParentType = "District";
		}
		else if (CurrentType == "Community" || CurrentType == "Custom" || CurrentType == "Road")
		{
			TargetParentType = "Street";
		}

		FString CurrentParentName = "None (无)";

		if (!TargetParentType.IsEmpty())
		{
			for (auto& Elem : WidgetMap)
			{
				UGISPolyItem* Item = Elem.Value;
				if (Item && Item->GetItemType() == TargetParentType)
				{
					FString OptionStr = FString::Printf(TEXT("%s [%s]"), *Item->GetItemName(), *Item->GetItemID());
					Edit_Input_Parent->AddOption(OptionStr);

					if (Item->GetItemID() == ItemToEdit->GetItemParentID())
					{
						CurrentParentName = OptionStr;
					}
				}
			}
		}
		Edit_Input_Parent->SetSelectedOption(CurrentParentName);
	}

	if (Edit_Dialog_Overlay)
	{
		Edit_Dialog_Overlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UGISWebWidget::SaveEditChanges()
{
	if (CurrentEditingItem.IsValid() && MapBrowser)
	{
		FString NewName = Edit_Input_Name->GetText().ToString();
		FString NewColor = Edit_Input_Color->GetText().ToString();
		FString NewOpacityStr = Edit_Input_Opacity->GetText().ToString();
		float NewOpacity = FCString::Atof(*NewOpacityStr);
		FString NewTextColor = Edit_Input_TextColor->GetText().ToString();
		FString ID = CurrentEditingItem->GetItemID();

		FString NewParentID = "None";
		if (Edit_Input_Parent)
		{
			FString SelectedStr = Edit_Input_Parent->GetSelectedOption();
			int32 LeftBracketIdx, RightBracketIdx;
			if (SelectedStr.FindChar('[', LeftBracketIdx) && SelectedStr.FindChar(']', RightBracketIdx))
			{
				NewParentID = SelectedStr.Mid(LeftBracketIdx + 1, RightBracketIdx - LeftBracketIdx - 1);
			}
		}

		FString OldParentID = CurrentEditingItem->GetItemParentID();
		if (NewParentID != OldParentID)
		{
			CurrentEditingItem->RemoveFromParent();
			UGISPolyItem** NewParentWidgetPtr = WidgetMap.Find(NewParentID);
			if (NewParentWidgetPtr && *NewParentWidgetPtr)
			{
				(*NewParentWidgetPtr)->AddChildItem(CurrentEditingItem.Get());
			}
			else
			{
				FString Type = CurrentEditingItem->GetItemType();
				if (Type == "Road" && List_Road)
				{
					List_Road->AddChild(CurrentEditingItem.Get());
				}
				else if (List_Admin)
				{
					List_Admin->AddChild(CurrentEditingItem.Get());
				}
			}
		}

		FString Script = FString::Printf(TEXT("updatePolyAttributes('%s', '%s', '%s', '%s', '%s', '%s');"),
		                                 *ID, *NewName, *NewColor, *NewOpacityStr, *NewTextColor, *NewParentID);

		MapBrowser->ExecuteJavascript(Script);

		CurrentEditingItem->UpdateData(NewName, NewColor, NewOpacity, NewTextColor, NewParentID);
	}
	CloseEditDialog();
}

void UGISWebWidget::CloseEditDialog()
{
	if (Edit_Dialog_Overlay)
	{
		Edit_Dialog_Overlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	CurrentEditingItem = nullptr;
}

void UGISWebWidget::OnColorSliderChanged(float Value)
{
	if (Slider_R && Slider_G && Slider_B)
	{
		FLinearColor NewColor(Slider_R->GetValue(), Slider_G->GetValue(), Slider_B->GetValue());
		if (Color_Preview)
		{
			Color_Preview->SetBrushColor(NewColor);
		}
		if (Edit_Input_Color)
		{
			Edit_Input_Color->SetText(FText::FromString("#" + NewColor.ToFColor(true).ToHex()));
		}
	}
}

void UGISWebWidget::OnTextColorSliderChanged(float Value)
{
	if (Slider_Text_R && Slider_Text_G && Slider_Text_B)
	{
		FLinearColor NewColor(Slider_Text_R->GetValue(), Slider_Text_G->GetValue(), Slider_Text_B->GetValue());
		if (TextColor_Preview)
		{
			TextColor_Preview->SetBrushColor(NewColor);
		}
		if (Edit_Input_TextColor)
		{
			Edit_Input_TextColor->SetText(FText::FromString("#" + NewColor.ToFColor(true).ToHex()));
		}
	}
}

void UGISWebWidget::OnPresetColorClicked(FLinearColor NewColor)
{
	UpdateColorUI(NewColor);
}

void UGISWebWidget::UpdateColorUI(FLinearColor Color)
{
	if (Slider_R) Slider_R->SetValue(Color.R);
	if (Slider_G) Slider_G->SetValue(Color.G);
	if (Slider_B) Slider_B->SetValue(Color.B);
	if (Color_Preview) Color_Preview->SetBrushColor(Color);
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

void UGISWebWidget::SetMode(FString ModeName)
{
	if (MapBrowser)
	{
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("setMode('%s');"), *ModeName));
	}
}

void UGISWebWidget::FocusID(FString ID)
{
	if (MapBrowser)
	{
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("focusPoly('%s');"), *ID));
	}
}

void UGISWebWidget::DeleteID(FString ID)
{
	if (MapBrowser)
	{
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("deletePoly('%s');"), *ID));
	}
	if (UGISPolyItem** Item = WidgetMap.Find(ID))
	{
		if (*Item)
		{
			(*Item)->RemoveFromParent();
		}
		WidgetMap.Remove(ID);
	}
}

void UGISWebWidget::FilterByType(FString TypeName)
{
	if (MapBrowser)
	{
		MapBrowser->ExecuteJavascript(FString::Printf(TEXT("filterPolys('%s');"), *TypeName));
	}
}

void UGISWebWidget::LoadMap(FString FileName)
{
	/* Legacy */
}

TArray<FString> UGISWebWidget::GetSaveFiles() { return TArray<FString>(); }

void UGISWebWidget::SaveMapAs(FString FileName)
{
	/* Legacy */
}

void UGISWebWidget::RequestSaveDataFromWeb()
{
	if (MapBrowser)
	{
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
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	FString NewGuid = FGuid::NewGuid().ToString();
	FString NowTime = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));

	RootObject->SetStringField("id", NewGuid);
	RootObject->SetStringField("name", SaveName);
	RootObject->SetStringField("desc", SaveDesc);
	RootObject->SetStringField("date", NowTime);

	TSharedPtr<FJsonValue> GeoJsonValue;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(GeoJsonData);

	if (FJsonSerializer::Deserialize(Reader, GeoJsonValue))
	{
		RootObject->SetField("data", GeoJsonValue);
	}
	else
	{
		RootObject->SetStringField("raw_data", GeoJsonData);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

	FString FullPath = FPaths::ProjectSavedDir() + TEXT("GISData/") + FString::Printf(
		TEXT("Save_%s_%s.json"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")), *NewGuid);
	FFileHelper::SaveStringToFile(OutputString, *FullPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
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

			if (List_Admin)
			{
				List_Admin->ClearChildren();
			}
			if (List_Reconstruct)
			{
				List_Reconstruct->ClearChildren();
			}
			if (List_Road)
			{
				List_Road->ClearChildren();
			}

			WidgetMap.Empty();
			LastProcessedID = "";

			MapDataStr = MapDataStr.Replace(TEXT("\n"), TEXT("")).Replace(TEXT("\r"), TEXT(""));
			if (MapBrowser)
			{
				MapBrowser->ExecuteJavascript(TEXT("importMap('") + MapDataStr + TEXT("');"));
			}
		}
	}
}

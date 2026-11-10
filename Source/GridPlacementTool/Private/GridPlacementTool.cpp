// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementTool.h"

#include "Editor.h"
#include "GridPlacementToolCommands.h"
#include "GridPlacementToolMenuHelpers.h"
#include "GridPlacementToolStyle.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogGridPlacementTool);

using namespace GridPlacementTool::Menu;

#define LOCTEXT_NAMESPACE "FGridPlacementToolModule"

void FGridPlacementToolModule::StartupModule()
{
	FGridPlacementToolStyle::Initialize();
	FGridPlacementToolStyle::ReloadTextures();
	FGridPlacementToolCommands::Register();

	BindCommands();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGridPlacementToolModule::RegisterMenus));
}

void FGridPlacementToolModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	FGridPlacementToolStyle::Shutdown();
}

void FGridPlacementToolModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);

	UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");

	FToolMenuEntry Entry = FToolMenuEntry::InitComboButton(
		"GridPlacementToolButton",
		FUIAction(
			FExecuteAction(),
			FCanExecuteAction::CreateStatic(&FGridPlacementToolModule::IsToolAllowedInCurrentEditorState),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateStatic(&FGridPlacementToolModule::IsToolAllowedInCurrentEditorState)),
		FOnGetContent::CreateRaw(this, &FGridPlacementToolModule::GenerateGridToolMenu),
		LOCTEXT("ToolbarButtonLabel", "Grid Placement"),
		LOCTEXT("ToolbarButtonToolTip", "Placing scene actors on a grid"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.LocationGridSnap"));

	Entry.StyleNameOverride = "CalloutToolbar";
	Section.AddEntry(Entry);
}

TSharedRef<SWidget> FGridPlacementToolModule::GenerateGridToolMenu()
{
	BuildMenuIfNeeded();
	return UToolMenus::Get()->GenerateWidget(GridPlacementToolMenuName, FToolMenuContext(UICommandList));
}

bool FGridPlacementToolModule::IsToolAllowedInCurrentEditorState()
{
	// Has no Play World
	return GEditor->PlayWorld == nullptr;
}

void FGridPlacementToolModule::BindCommands()
{
	check(!UICommandList.IsValid());
	UICommandList = MakeShareable(new FUICommandList);

	const FGridPlacementToolCommands& Commands = FGridPlacementToolCommands::Get();
	FUICommandList& ActionList = *UICommandList;

	ActionList.MapAction(
		Commands.UseFirstSelectedActor,
		FExecuteAction::CreateStatic(&InitialTransform::ToggleUseFirstSelectedActor),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&InitialTransform::IsUseFirstSelectedActor));

	ActionList.MapAction(
		Commands.AlignByObjectCenter,
		FExecuteAction::CreateStatic(&ObjectAlignment::ToggleAlignByObjectCenter),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&ObjectAlignment::IsAlignByObjectCenter));

	ActionList.MapAction(
		Commands.EnableSorting,
		FExecuteAction::CreateStatic(&Sorting::ToggleEnableSorting),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&Sorting::IsSortingEnabled));

	ActionList.MapAction(
		Commands.DescendingOrder,
		FExecuteAction::CreateStatic(&Sorting::ToggleDescendingOrder),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&Sorting::IsDescendingOrder));

	ActionList.MapAction(
		Commands.SizeBasedSpacing,
		FExecuteAction::CreateStatic(&SizeBasedSpacing::ToggleSizeBasedSpacing),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&SizeBasedSpacing::IsSizeBasedSpacing));

	ActionList.MapAction(
		Commands.FlowLayout,
		FExecuteAction::CreateStatic(&SizeBasedSpacing::ToggleFlowLayout),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&SizeBasedSpacing::IsFlowLayout));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGridPlacementToolModule, GridPlacementTool)

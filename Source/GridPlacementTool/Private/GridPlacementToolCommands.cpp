// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolCommands.h"

#define LOCTEXT_NAMESPACE "FGridPlacementToolModule"

void FGridPlacementToolCommands::RegisterCommands()
{
	UI_COMMAND(OpenMenu, "GridPlacementTool", "Open GridPlacementTool menu", EUserInterfaceActionType::Button, FInputChord());
	UI_COMMAND(UseFirstSelectedActor, "Use first selected actor", "If true, uses the transform of the first selected scene actor as the anchor origin for grid placement.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(AlignByObjectCenter, "Align by object center", "If true, aligns the object's bounds center to the grid node; otherwise the actor pivot is placed on the node.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(EnableSorting, "Enable sorting", "Whether to enable sorting of actors or assets before placing them on the grid", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(DescendingOrder, "Descending order", "If true and sorting is enabled, sorts items in descending order (from largest to smallest), otherwise in ascending order.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(SizeBasedSpacing, "Size-based spacing", "Objects are arranged on a grid based on their size; row depths or column width adjust to accommodate the largest objects.", EUserInterfaceActionType::ToggleButton, FInputChord());
	UI_COMMAND(FlowLayout, "Flow layout", "If true, calculates grid row layout based on the physical length of the first row.", EUserInterfaceActionType::ToggleButton, FInputChord());
}

const FGridPlacementToolCommands& FGridPlacementToolCommands::Get()
{
	return TCommands<FGridPlacementToolCommands>::Get();
}

#undef LOCTEXT_NAMESPACE

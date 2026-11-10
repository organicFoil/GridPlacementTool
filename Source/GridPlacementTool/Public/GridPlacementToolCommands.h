// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "Framework/Commands/Commands.h"
#include "GridPlacementToolStyle.h"

class FGridPlacementToolCommands : public TCommands<FGridPlacementToolCommands>
{
public:
	FGridPlacementToolCommands() : TCommands<FGridPlacementToolCommands>(
		TEXT("GridPlacementTool"),
		NSLOCTEXT("FGridPlacementToolModule", "GridPlacementToolCommands", "GridPlacementTool Plugin"),
		NAME_None,
		FGridPlacementToolStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;
	static const FGridPlacementToolCommands& Get();

	TSharedPtr<FUICommandInfo> OpenMenu;
	TSharedPtr<FUICommandInfo> EnableSorting;
	TSharedPtr<FUICommandInfo> DescendingOrder;
	TSharedPtr<FUICommandInfo> SizeBasedSpacing;
	TSharedPtr<FUICommandInfo> FlowLayout;
	TSharedPtr<FUICommandInfo> UseFirstSelectedActor;
	TSharedPtr<FUICommandInfo> AlignByObjectCenter;
};

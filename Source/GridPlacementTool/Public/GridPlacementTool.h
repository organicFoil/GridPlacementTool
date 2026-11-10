// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGridPlacementTool, Log, All);

class FGridPlacementToolModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void RegisterMenus();
	TSharedRef<SWidget> GenerateGridToolMenu();
	static bool IsToolAllowedInCurrentEditorState();
	void BindCommands();

	TSharedPtr<FUICommandList> UICommandList;
};

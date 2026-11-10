// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolSettings.h"

UGridPlacementToolSettings::UGridPlacementToolSettings()
{
	Spacing = FVector2D(-100.0f, 100.0f);
	Columns = 10;
	bUseFirstSelectedActor = true;
	bAlignByObjectCenter = true;
	InitialTransform = FEulerTransform::Identity;
	bEnableSorting = false;
	bDescendingOrder = false;
	bSizeBasedSpacing = false;
	bFlowLayout = false;
}

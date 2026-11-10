// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EulerTransform.h"
#include "UObject/Object.h"
#include "GridPlacementToolSettings.generated.h"

UCLASS(config=EditorPerProjectUserSettings, MinimalAPI)
class UGridPlacementToolSettings : public UObject
{
	GENERATED_BODY()

public:
	UGridPlacementToolSettings();

	/** Spacing between grid items. X represents up (row) spacing, Y represents right (column) spacing. The grid direction can be reversed by using a negative value. */
	UPROPERTY(Config)
	FVector2D Spacing;

	/** Number of columns (elements per row along the right axis) before wrapping to the next row. */
	UPROPERTY(Config)
	int32 Columns;

	/** Base world transform used as the origin when manual transform mode is active (bUseFirstSelectedActor is false). */
	UPROPERTY(Config)
	FEulerTransform InitialTransform;

	/** If true, uses the transform of the first selected scene actor as the anchor origin for grid placement. */
	UPROPERTY(Config)
	bool bUseFirstSelectedActor;

	/** If true, the object's bounds center is aligned to the grid node (pivot offset is compensated in the grid plane); otherwise the actor pivot (origin) is placed on the node. */
	UPROPERTY(Config)
	bool bAlignByObjectCenter;

	/** Whether to enable sorting of actors or assets before placing them on the grid. */
	UPROPERTY(Config)
	bool bEnableSorting;

	/** If true and sorting is enabled, sorts items in descending order (from largest to smallest), otherwise in ascending order. */
	UPROPERTY(Config)
	bool bDescendingOrder;

	/** Objects are arranged on a grid based on their size; row depths or column width adjust to accommodate the largest objects. */
	UPROPERTY(Config)
	bool bSizeBasedSpacing;

	/** If true, calculates grid row layout based on the physical length of the first row. */
	UPROPERTY(Config)
	bool bFlowLayout;
};

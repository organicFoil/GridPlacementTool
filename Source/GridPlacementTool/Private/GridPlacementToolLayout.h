// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"

class AActor;
class UGridPlacementToolSettings;

/** Layout strategy used to arrange items on the grid. */
enum class EGridLayoutMode : uint8
{
	/** Fixed grid of Rows x Columns. */
	Grid,

	/** Greedy rows that all fit into the physical width of the first row. */
	Flow
};

/** Immutable input parameters of a placement layout pass. */
struct FGridPlacementOptions
{
	/** Spacing between items. X is the row (forward) spacing, Y is the column (right) spacing. A negative value reverses the layout direction on that axis. */
	FVector2D Spacing = FVector2D(1.0, 1.0);

	/** Number of columns before wrapping to the next row. Values below 1 are clamped to 1. */
	int32 Columns = 1;

	/** World transform the layout is anchored to (its rotation defines the grid axes). */
	FTransform AnchorTransform = FTransform::Identity;

	/** If true, the item's bounds center lands on the grid node; otherwise the item pivot does. */
	bool bAlignByObjectCenter = false;

	/** If true, rows/columns grow to fit their largest item. */
	bool bSizeBasedSpacing = false;

	/** If true (and size-based spacing is on), rows match the physical width of the first row. */
	bool bFlowLayout = false;

	/** If true, items are sorted by their bounds area before being placed. */
	bool bEnableSorting = false;

	/** Sort direction for bEnableSorting: true is largest to smallest. */
	bool bDescendingOrder = false;

	static FGridPlacementOptions FromSettings(const UGridPlacementToolSettings& Settings);

	EGridLayoutMode ResolveLayoutMode() const
	{
		return (bFlowLayout && bSizeBasedSpacing) ? EGridLayoutMode::Flow : EGridLayoutMode::Grid;
	}
};

/** Outcome of a layout pass. Actors[i] pairs with Transforms[i]. */
struct FGridPlacementResult
{
	/** Items actually placed (invalid actors are dropped), in the order the transforms were computed for. */
	TArray<AActor*> Actors;

	/** Placement transform for each actor, including the actor's own scale. */
	TArray<FTransform> Transforms;

	/** True when sorting changed the item order, so the caller can mirror it in the editor selection. */
	bool bReordered = false;
};

namespace GridPlacement::Layout
{
	/**
	 * Computes placement transforms for the given actors.
	 *
	 * Pure computation: it neither mutates the input array nor any editor state. Invalid actors are
	 * skipped, so the result holds only placed items. When sorting is enabled the resulting order is
	 * reported through FGridPlacementResult.
	 */
	FGridPlacementResult Solve(TConstArrayView<AActor*> Actors, const FGridPlacementOptions& Options);
}

// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolLayout.h"

#include "GameFramework/Actor.h"
#include "GridPlacementToolSettings.h"

FGridPlacementOptions FGridPlacementOptions::FromSettings(const UGridPlacementToolSettings& Settings)
{
	FGridPlacementOptions Options;
	Options.Spacing = Settings.Spacing;
	Options.Columns = Settings.Columns;
	Options.AnchorTransform = Settings.InitialTransform.ToFTransform();
	Options.bAlignByObjectCenter = Settings.bAlignByObjectCenter;
	Options.bSizeBasedSpacing = Settings.bSizeBasedSpacing;
	Options.bFlowLayout = Settings.bFlowLayout;
	Options.bEnableSorting = Settings.bEnableSorting;
	Options.bDescendingOrder = Settings.bDescendingOrder;
	return Options;
}

namespace
{
	/** Asymmetric half-spans of a layout axis, indexed by row or column. */
	struct FSpans
	{
		TArray<double> Hi;
		TArray<double> Lo;

		explicit FSpans(int32 Count = 0)
		{
			Reset(Count);
		}

		void Reset(int32 Count)
		{
			Hi.Init(0.0, Count);
			Lo.Init(0.0, Count);
		}

		void Expand(int32 Index, double InHi, double InLo)
		{
			if (!Hi.IsValidIndex(Index))
			{
				return;
			}

			Hi[Index] = FMath::Max(Hi[Index], InHi);
			Lo[Index] = FMath::Max(Lo[Index], InLo);
		}

		double GetHi(int32 Index) const { return Hi.IsValidIndex(Index) ? Hi[Index] : 0.0; }
		double GetLo(int32 Index) const { return Lo.IsValidIndex(Index) ? Lo[Index] : 0.0; }
	};

	/**
	 * Per-item geometry, measured in the anchor's target frame.
	 * Hi is the distance from the item's reference point (pivot, or bounds center when center
	 * alignment is on) to its leading edge, Lo - to its trailing edge. An item therefore occupies
	 * [Reference - Lo, Reference + Hi] along each axis.
	 */
	struct FItem
	{
		AActor* Actor = nullptr;
		double DepthHi = 0.0;
		double DepthLo = 0.0;
		double WidthHi = 0.0;
		double WidthLo = 0.0;
		float BoundsArea = 0.0f;
		FVector RotatedCenterOffset = FVector::ZeroVector;
	};

	/** Advances a 1D cursor past one item and returns that item's reference position. */
	double AdvanceCursor(double& Cursor, double Direction, double Hi, double Lo, double Gap)
	{
		const double Reference = Direction > 0.0 ? (Cursor + Lo) : (Cursor - Hi);
		Cursor += Direction * (Hi + Lo + Gap);
		return Reference;
	}

	double GetAxisDirection(double Spacing)
	{
		return Spacing < 0.0 ? -1.0 : 1.0;
	}

	/**
	 * Collects geometry for every valid actor using actor-local component bounds.
	 * All measurements are taken in actor-local space (accounting for actor scale) and
	 * expressed in the anchor's target frame, making sorting and dimensions invariant
	 * to initial world rotations.
	 */
	void BuildItems(TConstArrayView<AActor*> Actors, const FGridPlacementOptions& Options, TArray<FItem>& OutItems)
	{
		OutItems.Reset();
		OutItems.Reserve(Actors.Num());

		const FQuat TargetRotation = Options.AnchorTransform.GetRotation();

		for (AActor* Actor : Actors)
		{
			if (!Actor)
			{
				continue;
			}

			FBox LocalBox = Actor->CalculateComponentsBoundingBoxInLocalSpace(false, false);
			if (!LocalBox.IsValid)
			{
				LocalBox = Actor->CalculateComponentsBoundingBoxInLocalSpace(true, false);
			}

			const FVector ActorScale = Actor->GetActorScale3D();
			const FVector ActorLocalCenterOffset = LocalBox.IsValid
				                                       ? (LocalBox.GetCenter() * ActorScale)
				                                       : FVector::ZeroVector;
			const FVector ActorLocalExtent = LocalBox.IsValid
				                                 ? (LocalBox.GetExtent() * ActorScale.GetAbs())
				                                 : FVector::ZeroVector;

			// When placed, the actor's local axes align with the target grid frame axes,
			// so the actor-local geometry is directly expressed in the target frame.
			const FVector CenterOffsetInTargetFrame = ActorLocalCenterOffset;
			const FVector Reference = Options.bAlignByObjectCenter ? FVector::ZeroVector : CenterOffsetInTargetFrame;

			FItem& Item = OutItems.AddDefaulted_GetRef();
			Item.Actor = Actor;
			Item.DepthHi = ActorLocalExtent.X + Reference.X;
			Item.DepthLo = ActorLocalExtent.X - Reference.X;
			Item.WidthHi = ActorLocalExtent.Y + Reference.Y;
			Item.WidthLo = ActorLocalExtent.Y - Reference.Y;
			Item.BoundsArea = static_cast<float>((ActorLocalExtent.X * 2.0) * (ActorLocalExtent.Y * 2.0));

			// Rotated world offset used to snap the bounds center onto the node. Only the in-plane
			// (X/Y) part is compensated so the pivot keeps its height.
			FVector RotatedCenterOffset = TargetRotation.RotateVector(ActorLocalCenterOffset);
			RotatedCenterOffset.Z = 0.0;
			Item.RotatedCenterOffset = RotatedCenterOffset;
		}
	}

	void SortItemsByArea(TArray<FItem>& Items, bool bDescending)
	{
		Items.StableSort(
			[bDescending](const FItem& Lhs, const FItem& Rhs)
			{
				return bDescending ? Lhs.BoundsArea > Rhs.BoundsArea : Lhs.BoundsArea < Rhs.BoundsArea;
			});
	}

	/** Fixed grid: item Index lands in row Index / Columns and column Index % Columns. */
	void ComputeGridCenters(const TArray<FItem>& Items, const FGridPlacementOptions& Options, TArray<FVector>& OutCenters)
	{
		const int32 NumItems = Items.Num();
		OutCenters.Reset();
		OutCenters.SetNum(NumItems);

		if (NumItems == 0)
		{
			return;
		}

		const int32 SafeColumns = FMath::Max(1, Options.Columns);
		const int32 NumRows = (NumItems + SafeColumns - 1) / SafeColumns;
		const double GapX = FMath::Abs(Options.Spacing.X);
		const double GapY = FMath::Abs(Options.Spacing.Y);
		const double DirectionX = GetAxisDirection(Options.Spacing.X);
		const double DirectionY = GetAxisDirection(Options.Spacing.Y);

		// Size-based spacing grows every row/column to fit its largest item. With uniform spacing
		// the spans stay zero and the cursor degenerates to Index * Spacing.
		FSpans RowSpans(NumRows);
		FSpans ColumnSpans(SafeColumns);
		if (Options.bSizeBasedSpacing)
		{
			for (int32 Index = 0; Index < NumItems; ++Index)
			{
				RowSpans.Expand(Index / SafeColumns, Items[Index].DepthHi, Items[Index].DepthLo);
				ColumnSpans.Expand(Index % SafeColumns, Items[Index].WidthHi, Items[Index].WidthLo);
			}
		}

		TArray<double> RowPositions;
		RowPositions.SetNum(NumRows);
		double CursorX = 0.0;
		for (int32 Row = 0; Row < NumRows; ++Row)
		{
			RowPositions[Row] = AdvanceCursor(CursorX, DirectionX, RowSpans.GetHi(Row), RowSpans.GetLo(Row), GapX);
		}

		TArray<double> ColumnPositions;
		ColumnPositions.SetNum(SafeColumns);
		double CursorY = 0.0;
		for (int32 Column = 0; Column < SafeColumns; ++Column)
		{
			ColumnPositions[Column] = AdvanceCursor(CursorY, DirectionY, ColumnSpans.GetHi(Column), ColumnSpans.GetLo(Column), GapY);
		}

		for (int32 Index = 0; Index < NumItems; ++Index)
		{
			OutCenters[Index] = FVector(RowPositions[Index / SafeColumns], ColumnPositions[Index % SafeColumns], 0.0);
		}
	}

	/** Greedy rows packed into the physical width of the first row. */
	void ComputeFlowLayoutCenters(const TArray<FItem>& Items, const FGridPlacementOptions& Options, TArray<FVector>& OutCenters)
	{
		const int32 NumItems = Items.Num();
		OutCenters.Reset();
		OutCenters.SetNum(NumItems);

		if (NumItems == 0)
		{
			return;
		}

		const int32 SafeColumns = FMath::Max(1, Options.Columns);
		const double GapX = FMath::Abs(Options.Spacing.X);
		const double GapY = FMath::Abs(Options.Spacing.Y);
		const double DirectionX = GetAxisDirection(Options.Spacing.X);
		const double DirectionY = GetAxisDirection(Options.Spacing.Y);

		// The first (up to Columns) items define the row width every next row has to fit into.
		const int32 FirstRowCount = FMath::Min(SafeColumns, NumItems);
		double MaxRowWidth = GapY * (FirstRowCount - 1);
		for (int32 Index = 0; Index < FirstRowCount; ++Index)
		{
			MaxRowWidth += Items[Index].WidthHi + Items[Index].WidthLo;
		}

		// Distribute items into rows and compute their positions in the anchor's target frame.
		int32 Index = 0;
		double CursorX = 0.0;

		while (Index < NumItems)
		{
			const int32 RowStart = Index;
			const bool bFirstRow = (RowStart == 0);
			double RowDepthHi = Items[Index].DepthHi;
			double RowDepthLo = Items[Index].DepthLo;
			double RowWidth = Items[Index].WidthHi + Items[Index].WidthLo;
			++Index;

			while (Index < NumItems)
			{
				if (bFirstRow)
				{
					if (Index - RowStart >= FirstRowCount)
					{
						break;
					}
				}
				else
				{
					const double NewWidth = RowWidth + GapY + Items[Index].WidthHi + Items[Index].WidthLo;
					if (NewWidth > MaxRowWidth + UE_KINDA_SMALL_NUMBER)
					{
						break;
					}
				}

				RowWidth += GapY + Items[Index].WidthHi + Items[Index].WidthLo;
				RowDepthHi = FMath::Max(RowDepthHi, Items[Index].DepthHi);
				RowDepthLo = FMath::Max(RowDepthLo, Items[Index].DepthLo);
				++Index;
			}

			const double RowX = AdvanceCursor(CursorX, DirectionX, RowDepthHi, RowDepthLo, GapX);

			double CursorY = 0.0;
			for (int32 ItemIndex = RowStart; ItemIndex < Index; ++ItemIndex)
			{
				const double ItemY = AdvanceCursor(CursorY, DirectionY, Items[ItemIndex].WidthHi, Items[ItemIndex].WidthLo, GapY);
				OutCenters[ItemIndex] = FVector(RowX, ItemY, 0.0);
			}
		}
	}

	/** Places the computed positions into world space, keeping the first item's reference point on the anchor. */
	void BuildResult(
		const TArray<FItem>& Items,
		const FGridPlacementOptions& Options,
		const TArray<FVector>& LocalCenters,
		FGridPlacementResult& OutResult)
	{
		const FQuat TargetRotation = Options.AnchorTransform.GetRotation();
		const FVector AnchorLocation = Options.AnchorTransform.GetLocation();
		const FVector AnchorOffset = LocalCenters.IsEmpty() ? FVector::ZeroVector : LocalCenters[0];

		OutResult.Actors.Reserve(Items.Num());
		OutResult.Transforms.Reserve(Items.Num());

		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			const FItem& Item = Items[Index];

			FVector Delta = TargetRotation.RotateVector(LocalCenters[Index] - AnchorOffset);
			Delta.Z = 0.0;

			// Keep the pivot at the anchor height: with center alignment only the in-plane part
			// of the center offset is compensated.
			const FVector TargetLocation = Options.bAlignByObjectCenter
				                               ? (AnchorLocation + Delta - Item.RotatedCenterOffset)
				                               : (AnchorLocation + Delta);

			OutResult.Actors.Add(Item.Actor);
			OutResult.Transforms.Add(FTransform(TargetRotation, TargetLocation, Item.Actor->GetActorTransform().GetScale3D()));
		}
	}
}

FGridPlacementResult GridPlacement::Layout::Solve(TConstArrayView<AActor*> Actors, const FGridPlacementOptions& Options)
{
	FGridPlacementResult Result;

	TArray<FItem> Items;
	BuildItems(Actors, Options, Items);

	if (Options.bEnableSorting && Items.Num() >= 2)
	{
		TArray<AActor*> OriginalOrder;
		OriginalOrder.Reserve(Items.Num());
		for (const FItem& Item : Items)
		{
			OriginalOrder.Add(Item.Actor);
		}

		SortItemsByArea(Items, Options.bDescendingOrder);

		for (int32 Index = 0; Index < Items.Num(); ++Index)
		{
			if (Items[Index].Actor != OriginalOrder[Index])
			{
				Result.bReordered = true;
				break;
			}
		}
	}

	TArray<FVector> LocalCenters;
	if (Options.ResolveLayoutMode() == EGridLayoutMode::Flow)
	{
		ComputeFlowLayoutCenters(Items, Options, LocalCenters);
	}
	else
	{
		ComputeGridCenters(Items, Options, LocalCenters);
	}

	BuildResult(Items, Options, LocalCenters, Result);
	return Result;
}

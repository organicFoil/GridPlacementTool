// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolLayout.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GridPlacementToolTestHelpers.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"

using namespace GridPlacementTool::Test;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementLayoutSolveTest,
	"GridPlacementTool.Layout.Solve",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGridPlacementLayoutSolveTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	const FVector BoxExtent(10.0, 10.0, 10.0);

	// Pivot-centered items, used by the plain grid checks.
	TArray<AActor*> CenteredActors = SpawnBoxActors(*World, {BoxExtent, BoxExtent, BoxExtent, BoxExtent});

	// Bounds center offset from the pivot by (5, -5, 0) and the actor is scaled x2,
	// so the world space center offset is (10, -10, 0).
	TArray<AActor*> OffCenterActors = SpawnBoxActors(
		*World,
		{BoxExtent, BoxExtent, BoxExtent, BoxExtent},
		FVector(5.0, -5.0, 0.0),
		FVector(2.0));

	// Fixture sanity: the measured bounds must be exactly the box we asked for.
	{
		FVector Origin, Extent;
		OffCenterActors[0]->GetActorBounds(true, Origin, Extent);
		TestEqual(TEXT("Fixture: off-center bounds origin"), Origin, FVector(10.0, -10.0, 0.0), PositionTolerance);
		TestEqual(TEXT("Fixture: off-center bounds extent"), Extent, FVector(20.0, 20.0, 20.0), PositionTolerance);
	}

	// 1. Fixed grid mapping: index -> (row, column) with uniform spacing.
	{
		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(CenteredActors, Options);

		ExpectLocations(
			*this,
			TEXT("Uniform spacing"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(100.0, 0.0, 0.0), FVector(100.0, 200.0, 0.0)});

		TestFalse(TEXT("Uniform spacing: not reordered"), Result.bReordered);
		TestTrue(TEXT("Uniform spacing: source order preserved"), Result.Actors == CenteredActors);
	}

	// 2. Center alignment snaps the bounds center onto the node, scale included.
	{
		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;
		Options.bAlignByObjectCenter = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(OffCenterActors, Options);

		ExpectLocations(
			*this,
			TEXT("Center alignment"),
			Result,
			{FVector(-10.0, 10.0, 0.0), FVector(-10.0, 210.0, 0.0), FVector(90.0, 10.0, 0.0), FVector(90.0, 210.0, 0.0)});

		for (int32 Index = 0; Index < Result.Transforms.Num(); ++Index)
		{
			TestEqual(
				FString::Printf(TEXT("Center alignment: item %d scale preserved"), Index),
				Result.Transforms[Index].GetScale3D(),
				FVector(2.0),
				PositionTolerance);
		}
	}

	// 3. A negative spacing flips the layout direction.
	{
		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(-100.0, -200.0);
		Options.Columns = 2;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(CenteredActors, Options);

		ExpectLocations(
			*this,
			TEXT("Reversed spacing"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, -200.0, 0.0), FVector(-100.0, 0.0, 0.0), FVector(-100.0, -200.0, 0.0)});
	}

	// 4. A rotated anchor rotates the whole grid.
	{
		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;
		Options.AnchorTransform = FTransform(FRotator(0.0, 90.0, 0.0), FVector::ZeroVector);

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(CenteredActors, Options);

		ExpectLocations(
			*this,
			TEXT("Yaw 90 anchor"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(-200.0, 0.0, 0.0), FVector(0.0, 100.0, 0.0), FVector(-200.0, 100.0, 0.0)});
	}

	// 5. Rotated anchor with center alignment aligns off-center object bounds centers to rotated grid nodes.
	{
		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;
		Options.AnchorTransform = FTransform(FRotator(0.0, 90.0, 0.0), FVector::ZeroVector);
		Options.bAlignByObjectCenter = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(OffCenterActors, Options);

		ExpectLocations(
			*this,
			TEXT("Yaw 90 anchor with center alignment"),
			Result,
			{FVector(-10.0, -10.0, 0.0), FVector(-210.0, -10.0, 0.0), FVector(-10.0, 90.0, 0.0), FVector(-210.0, 90.0, 0.0)});

		// Verify bounds center after applying transform matches grid nodes exactly.
		const TArray<FVector> ExpectedNodes = {
			FVector(0.0, 0.0, 0.0),
			FVector(-200.0, 0.0, 0.0),
			FVector(0.0, 100.0, 0.0),
			FVector(-200.0, 100.0, 0.0)
		};
		for (int32 Index = 0; Index < Result.Transforms.Num(); ++Index)
		{
			AActor* Actor = Result.Actors[Index];
			const FTransform OriginalTransform = Actor->GetActorTransform();
			Actor->SetActorTransform(Result.Transforms[Index]);

			FVector Origin, Extent;
			Actor->GetActorBounds(true, Origin, Extent);
			TestEqual(
				FString::Printf(TEXT("Yaw 90 anchor: item %d bounds center"), Index),
				Origin,
				ExpectedNodes[Index],
				PositionTolerance);

			Actor->SetActorTransform(OriginalTransform);
		}
	}

	// 6. Pre-rotated scene actors do not distort center alignment when placed on a rotated anchor.
	{
		TArray<AActor*> PreRotatedActors = SpawnBoxActors(
			*World,
			{BoxExtent, BoxExtent, BoxExtent, BoxExtent},
			FVector(5.0, -5.0, 0.0),
			FVector(2.0));

		for (int32 Index = 0; Index < PreRotatedActors.Num(); ++Index)
		{
			PreRotatedActors[Index]->SetActorRotation(FRotator(0.0, 90.0 * (Index + 1), 0.0));
		}

		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;
		Options.AnchorTransform = FTransform(FRotator(0.0, 90.0, 0.0), FVector::ZeroVector);
		Options.bAlignByObjectCenter = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(PreRotatedActors, Options);

		ExpectLocations(
			*this,
			TEXT("Pre-rotated actors with rotated anchor"),
			Result,
			{FVector(-10.0, -10.0, 0.0), FVector(-210.0, -10.0, 0.0), FVector(-10.0, 90.0, 0.0), FVector(-210.0, 90.0, 0.0)});

		// Repeat placement idempotency: running solution again on placed actors produces identical transforms.
		for (int32 Index = 0; Index < Result.Transforms.Num(); ++Index)
		{
			PreRotatedActors[Index]->SetActorTransform(Result.Transforms[Index]);
		}
		const FGridPlacementResult SecondPassResult = GridPlacement::Layout::Solve(PreRotatedActors, Options);
		ExpectLocations(
			*this,
			TEXT("Repeat placement idempotency"),
			SecondPassResult,
			{FVector(-10.0, -10.0, 0.0), FVector(-210.0, -10.0, 0.0), FVector(-10.0, 90.0, 0.0), FVector(-210.0, 90.0, 0.0)});
	}

	// 7. Invalid actors are dropped instead of leaving holes in the grid.
	{
		TArray<AActor*> ActorsWithHole = {CenteredActors[0], nullptr, CenteredActors[1]};

		FGridPlacementOptions Options;
		Options.Spacing = FVector2D(100.0, 200.0);
		Options.Columns = 2;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(ActorsWithHole, Options);

		ExpectLocations(*this, TEXT("Dropped invalid actor"), Result, {FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0)});
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementSizeBasedSpacingTest,
	"GridPlacementTool.Layout.SizeBasedSpacing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGridPlacementSizeBasedSpacingTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	// Depth (X) and width (Y) differ per item, so the row and column spans have to grow.
	// Row 0 holds items 0-1 (max depth 20), row 1 holds items 2-3 (max depth 40).
	// Column 0 holds items 0 and 2 (max width 20), column 1 holds items 1 and 3 (max width 50).
	TArray<AActor*> Actors = SpawnBoxActors(
		*World,
		{
			FVector(10.0, 10.0, 10.0),
			FVector(20.0, 30.0, 10.0),
			FVector(40.0, 20.0, 10.0),
			FVector(10.0, 50.0, 10.0)
		});

	FGridPlacementOptions Options;
	Options.Spacing = FVector2D(100.0, 200.0);
	Options.Columns = 2;

	// Uniform spacing ignores the item sizes.
	{
		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		ExpectLocations(
			*this,
			TEXT("Uniform"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(100.0, 0.0, 0.0), FVector(100.0, 200.0, 0.0)});
	}

	// Size-based spacing: rows step by 20 + 40 + spacing, columns by 20 + 50 + spacing.
	{
		Options.bSizeBasedSpacing = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		ExpectLocations(
			*this,
			TEXT("Size based"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 270.0, 0.0), FVector(160.0, 0.0, 0.0), FVector(160.0, 270.0, 0.0)});
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementFlowLayoutTest,
	"GridPlacementTool.Layout.FlowLayout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGridPlacementFlowLayoutTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	// The first three items define a row width of 2 * 200 gap + 3 * 40 item width = 520.
	// The last two items are 120 and 220 wide, and 120 + 200 + 220 exceeds that limit,
	// so they end up on separate rows even though Columns would allow them to share one.
	TArray<AActor*> Actors = SpawnBoxActors(
		*World,
		{
			FVector(10.0, 20.0, 10.0),
			FVector(10.0, 20.0, 10.0),
			FVector(10.0, 20.0, 10.0),
			FVector(10.0, 60.0, 10.0),
			FVector(10.0, 110.0, 10.0)
		});

	FGridPlacementOptions Options;
	Options.Spacing = FVector2D(100.0, 200.0);
	Options.Columns = 3;
	Options.bSizeBasedSpacing = true;
	Options.bFlowLayout = true;

	// Rows: [0, 1, 2], [3], [4].
	{
		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		ExpectLocations(
			*this,
			TEXT("Flow layout"),
			Result,
			{
				FVector(0.0, 0.0, 0.0),
				FVector(0.0, 240.0, 0.0),
				FVector(0.0, 480.0, 0.0),
				FVector(120.0, 40.0, 0.0),
				FVector(240.0, 90.0, 0.0)
			});
	}

	// Without size-based spacing the flow layout option is ignored and the plain grid is used.
	{
		Options.bSizeBasedSpacing = false;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		ExpectLocations(
			*this,
			TEXT("Flow layout ignored"),
			Result,
			{
				FVector(0.0, 0.0, 0.0),
				FVector(0.0, 200.0, 0.0),
				FVector(0.0, 400.0, 0.0),
				FVector(100.0, 0.0, 0.0),
				FVector(100.0, 200.0, 0.0)
			});
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementSortingTest,
	"GridPlacementTool.Layout.Sorting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGridPlacementSortingTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	// Bounds areas are 400, 400, 800 and 1200; the first two tie, which exercises the stable sort.
	TArray<AActor*> Actors = SpawnBoxActors(
		*World,
		{
			FVector(10.0, 10.0, 10.0),
			FVector(10.0, 10.0, 10.0),
			FVector(20.0, 10.0, 10.0),
			FVector(30.0, 10.0, 10.0)
		});

	FGridPlacementOptions Options;
	Options.Spacing = FVector2D(100.0, 200.0);
	Options.Columns = 4;
	Options.bEnableSorting = true;

	// Already ascending: sorting must not touch the order.
	{
		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		TestFalse(TEXT("Ascending: not reordered"), Result.bReordered);
		TestTrue(TEXT("Ascending: order preserved"), Result.Actors == Actors);
		ExpectLocations(
			*this,
			TEXT("Ascending"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(0.0, 400.0, 0.0), FVector(0.0, 600.0, 0.0)});
	}

	// Descending: the largest item goes first and the tied pair keeps its relative order.
	{
		Options.bDescendingOrder = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, Options);

		const TArray<AActor*> ExpectedOrder = {Actors[3], Actors[2], Actors[0], Actors[1]};
		TestTrue(TEXT("Descending: reordered flag"), Result.bReordered);
		TestTrue(TEXT("Descending: order"), Result.Actors == ExpectedOrder);
		ExpectLocations(
			*this,
			TEXT("Descending"),
			Result,
			{FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(0.0, 400.0, 0.0), FVector(0.0, 600.0, 0.0)});
	}

	// Asymmetric actors with varying aspect ratios:
	// A: 10x30 (footprint 20x60 = 1200)
	// B: 20x10 (footprint 40x20 = 800)
	// C: 40x5  (footprint 80x10 = 800, ties with B)
	// D: 5x5   (footprint 10x10 = 100)
	TArray<AActor*> AsymmetricActors = SpawnBoxActors(
		*World,
		{
			FVector(10.0, 30.0, 10.0), // 0: Area = 1200
			FVector(20.0, 10.0, 10.0), // 1: Area = 800
			FVector(40.0, 5.0, 10.0), // 2: Area = 800
			FVector(5.0, 5.0, 10.0) // 3: Area = 100
		});

	const TArray<AActor*> ExpectedAscending = {AsymmetricActors[3], AsymmetricActors[1], AsymmetricActors[2], AsymmetricActors[0]};
	const TArray<AActor*> ExpectedDescending = {AsymmetricActors[0], AsymmetricActors[1], AsymmetricActors[2], AsymmetricActors[3]};

	const TArray<FRotator> TestRotations = {
		FRotator(0.0, 0.0, 0.0),
		FRotator(0.0, 30.0, 0.0),
		FRotator(0.0, 45.0, 0.0),
		FRotator(0.0, 90.0, 0.0),
		FRotator(0.0, 180.0, 0.0),
		FRotator(30.0, 45.0, 60.0)
	};

	// 1. Invariance across InitialTransform rotations.
	for (const FRotator& Rotation : TestRotations)
	{
		FGridPlacementOptions RotOptions;
		RotOptions.Spacing = FVector2D(100.0, 200.0);
		RotOptions.Columns = 4;
		RotOptions.bEnableSorting = true;
		RotOptions.AnchorTransform = FTransform(Rotation, FVector::ZeroVector);

		// Ascending order across rotations
		{
			RotOptions.bDescendingOrder = false;
			const FGridPlacementResult Result = GridPlacement::Layout::Solve(AsymmetricActors, RotOptions);
			TestTrue(
				FString::Printf(TEXT("Rotated anchor (%s) ascending sort order"), *Rotation.ToString()),
				Result.Actors == ExpectedAscending);
		}

		// Descending order across rotations
		{
			RotOptions.bDescendingOrder = true;
			const FGridPlacementResult Result = GridPlacement::Layout::Solve(AsymmetricActors, RotOptions);
			TestTrue(
				FString::Printf(TEXT("Rotated anchor (%s) descending sort order"), *Rotation.ToString()),
				Result.Actors == ExpectedDescending);
		}
	}

	// 2. Invariance with pre-rotated actors in world space.
	for (int32 Index = 0; Index < AsymmetricActors.Num(); ++Index)
	{
		AsymmetricActors[Index]->SetActorRotation(FRotator(15.0 * (Index + 1), 37.0 * (Index + 1), 22.0 * (Index + 1)));
	}

	for (const FRotator& Rotation : TestRotations)
	{
		FGridPlacementOptions RotOptions;
		RotOptions.Spacing = FVector2D(100.0, 200.0);
		RotOptions.Columns = 4;
		RotOptions.bEnableSorting = true;
		RotOptions.AnchorTransform = FTransform(Rotation, FVector::ZeroVector);

		// Ascending with pre-rotated actors
		{
			RotOptions.bDescendingOrder = false;
			const FGridPlacementResult Result = GridPlacement::Layout::Solve(AsymmetricActors, RotOptions);
			TestTrue(
				FString::Printf(TEXT("Pre-rotated actors with anchor (%s) ascending sort order"), *Rotation.ToString()),
				Result.Actors == ExpectedAscending);
		}

		// Descending with pre-rotated actors
		{
			RotOptions.bDescendingOrder = true;
			const FGridPlacementResult Result = GridPlacement::Layout::Solve(AsymmetricActors, RotOptions);
			TestTrue(
				FString::Printf(TEXT("Pre-rotated actors with anchor (%s) descending sort order"), *Rotation.ToString()),
				Result.Actors == ExpectedDescending);

			// Idempotency: re-solving after applying transforms preserves order and produces identical transforms
			for (int32 Index = 0; Index < Result.Transforms.Num(); ++Index)
			{
				Result.Actors[Index]->SetActorTransform(Result.Transforms[Index]);
			}
			const FGridPlacementResult SecondPassResult = GridPlacement::Layout::Solve(Result.Actors, RotOptions);
			TestTrue(
				FString::Printf(TEXT("Idempotency sort order (%s)"), *Rotation.ToString()),
				SecondPassResult.Actors == ExpectedDescending);
			ExpectLocations(*this, FString::Printf(TEXT("Idempotency transforms (%s)"), *Rotation.ToString()), SecondPassResult, {
				                Result.Transforms[0].GetLocation(),
				                Result.Transforms[1].GetLocation(),
				                Result.Transforms[2].GetLocation(),
				                Result.Transforms[3].GetLocation()
			                });
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementEdgeCasesTest,
	"GridPlacementTool.Layout.EdgeCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGridPlacementEdgeCasesTest::RunTest(const FString& Parameters)
{
	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	FGridPlacementOptions Options;
	Options.Spacing = FVector2D(100.0, 200.0);
	Options.Columns = 2;

	// No actors at all.
	{
		TArray<AActor*> NoActors;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(NoActors, Options);

		TestEqual(TEXT("Empty input: item count"), Result.Transforms.Num(), 0);
		TestFalse(TEXT("Empty input: not reordered"), Result.bReordered);
	}

	// Nothing but invalid actors.
	{
		TArray<AActor*> InvalidActors = {nullptr, nullptr};

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(InvalidActors, Options);

		TestEqual(TEXT("Invalid input: item count"), Result.Transforms.Num(), 0);
	}

	// A single item lands on the anchor, with an off-center pivot compensated around it.
	{
		TArray<AActor*> SingleActor = SpawnBoxActors(*World, {FVector(10.0, 10.0, 10.0)}, FVector(5.0, -5.0, 0.0));

		FGridPlacementOptions AnchorOptions = Options;
		AnchorOptions.bAlignByObjectCenter = true;
		AnchorOptions.AnchorTransform = FTransform(FRotator::ZeroRotator, FVector(1000.0, 2000.0, 3000.0));

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(SingleActor, AnchorOptions);

		ExpectLocations(*this, TEXT("Single item"), Result, {FVector(995.0, 2005.0, 3000.0)});
	}

	// A single item with size-based spacing must not produce NaN offsets.
	{
		TArray<AActor*> SingleActor = SpawnBoxActors(*World, {FVector(10.0, 10.0, 10.0)});

		FGridPlacementOptions SizeOptions = Options;
		SizeOptions.bSizeBasedSpacing = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(SingleActor, SizeOptions);

		ExpectLocations(*this, TEXT("Single item size based"), Result, {FVector(0.0, 0.0, 0.0)});
	}

	// Sorting is ignored for a single item.
	{
		TArray<AActor*> SingleActor = SpawnBoxActors(*World, {FVector(10.0, 10.0, 10.0)});

		FGridPlacementOptions SortOptions = Options;
		SortOptions.bEnableSorting = true;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(SingleActor, SortOptions);

		ExpectLocations(*this, TEXT("Single item sorted"), Result, {FVector(0.0, 0.0, 0.0)});
		TestFalse(TEXT("Single item sorted: not reordered"), Result.bReordered);
	}

	// More columns than items collapses into a single row.
	{
		TArray<AActor*> Actors = SpawnBoxActors(*World, {FVector(10.0, 10.0, 10.0), FVector(10.0, 10.0, 10.0)});

		FGridPlacementOptions WideOptions = Options;
		WideOptions.Columns = 5;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, WideOptions);

		ExpectLocations(*this, TEXT("Columns above count"), Result, {FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0)});
	}

	// A non-positive column count is clamped to one column per row.
	{
		TArray<AActor*> Actors = SpawnBoxActors(*World, {FVector(10.0, 10.0, 10.0), FVector(10.0, 10.0, 10.0)});

		FGridPlacementOptions ZeroOptions = Options;
		ZeroOptions.Columns = 0;

		const FGridPlacementResult Result = GridPlacement::Layout::Solve(Actors, ZeroOptions);

		ExpectLocations(*this, TEXT("Clamped columns"), Result, {FVector(0.0, 0.0, 0.0), FVector(100.0, 0.0, 0.0)});
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

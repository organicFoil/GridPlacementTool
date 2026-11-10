// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolMenuHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "GridPlacementToolTestHelpers.h"

#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

using namespace GridPlacementTool::Test;

namespace Menu = GridPlacementTool::Menu;

namespace
{
	/** Scene actors currently selected in the editor, in the order the editor reports them. */
	TArray<AActor*> GetEditorSelection()
	{
		return UEditorUtilityLibrary::GetSelectionSet();
	}

	/** A throwaway asset object, standing in for a Content Browser selection. */
	UObject* MakeAsset()
	{
		// UObject itself is abstract, so a concrete UObject type is required here.
		return NewObject<UStaticMesh>(GetTransientPackage());
	}

	/**
	 * Actors spawned into the editor world by a test, removed again when the object goes out of scope.
	 * They are transient so running the tests never leaves traces in the level.
	 */
	struct FScopedEditorActors
	{
		explicit FScopedEditorActors(UWorld& InWorld)
			: World(InWorld)
		{
		}

		~FScopedEditorActors()
		{
			GEditor->SelectNone(false, true);
			for (AActor* Actor : Actors)
			{
				if (IsValid(Actor))
				{
					World.EditorDestroyActor(Actor, /*bShouldModifyLevel=*/false);
				}
			}
		}

		AActor* AddBox(const FVector& BoxExtent)
		{
			AActor* Actor = SpawnBoxActor(World, BoxExtent, FVector::ZeroVector, FVector::OneVector, /*bTransient=*/false);
			Actors.Add(Actor);
			return Actor;
		}

		/** Takes ownership of actors spawned by the code under test, so they are cleaned up as well. */
		void Track(const TArray<AActor*>& InActors)
		{
			Actors.Append(InActors);
		}

		TArray<AActor*> Actors;

	private:
		UWorld& World;
	};

	/** Asserts that actors sit at the expected locations, in the given order. */
	void ExpectActorLocations(
		FAutomationTestBase& Test,
		const FString& Label,
		const TArray<AActor*>& Actors,
		const TArray<FVector>& Expected)
	{
		Test.TestEqual(Label + TEXT(": actor count"), Actors.Num(), Expected.Num());
		for (int32 Index = 0; Index < FMath::Min(Actors.Num(), Expected.Num()); ++Index)
		{
			Test.TestEqual(
				FString::Printf(TEXT("%s: actor %d"), *Label, Index),
				Actors[Index]->GetActorLocation(),
				Expected[Index],
				PositionTolerance);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementMenuOptionsTest,
	"GridPlacementTool.Menu.BuildPlacementOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** The tool settings must reach the solver untouched, and the anchor resolution has to stay ordered. */
bool FGridPlacementMenuOptionsTest::RunTest(const FString& Parameters)
{
	FScopedSettingsOverride SettingsOverride;
	UGridPlacementToolSettings& Settings = SettingsOverride.Get();

	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	// The first actor is deliberately the smaller one, so the anchor cannot be mistaken for a sort result.
	TArray<AActor*> SelectedActors = SpawnBoxActors(*World, { FVector(10.0, 10.0, 10.0), FVector(30.0, 10.0, 10.0) });
	SelectedActors[0]->SetActorLocationAndRotation(FVector(100.0, 200.0, 300.0), FRotator(0.0, 90.0, 0.0));
	SelectedActors[1]->SetActorLocationAndRotation(FVector(-100.0, -200.0, -300.0), FRotator::ZeroRotator);

	Settings.Spacing = FVector2D(11.0, 22.0);
	Settings.Columns = 5;
	Settings.InitialTransform = FEulerTransform(FVector(1.0, 2.0, 3.0), FRotator::ZeroRotator, FVector::OneVector);
	Settings.bAlignByObjectCenter = true;
	Settings.bSizeBasedSpacing = true;
	Settings.bFlowLayout = true;
	Settings.bEnableSorting = true;
	Settings.bDescendingOrder = true;
	Settings.bUseFirstSelectedActor = false;

	// Every setting is forwarded, and the manual transform is the anchor while the tool is not using an actor.
	{
		const FPlacementSelection Empty;
		const FGridPlacementOptions Options = Menu::BuildPlacementOptions(Empty);

		TestTrue(TEXT("Spacing"), Options.Spacing == FVector2D(11.0, 22.0));
		TestEqual(TEXT("Columns"), Options.Columns, 5);
		TestEqual(TEXT("Manual anchor location"), Options.AnchorTransform.GetLocation(), FVector(1.0, 2.0, 3.0), PositionTolerance);
		TestTrue(TEXT("bAlignByObjectCenter"), Options.bAlignByObjectCenter);
		TestTrue(TEXT("bSizeBasedSpacing"), Options.bSizeBasedSpacing);
		TestTrue(TEXT("bFlowLayout"), Options.bFlowLayout);
		TestTrue(TEXT("bEnableSorting"), Options.bEnableSorting);
		TestTrue(TEXT("bDescendingOrder"), Options.bDescendingOrder);
		TestTrue(TEXT("Flow layout mode"), Options.ResolveLayoutMode() == EGridLayoutMode::Flow);
	}

	// With the anchor setting on, the anchor is the first actor in selection order, whatever its size.
	{
		Settings.bUseFirstSelectedActor = true;

		FPlacementSelection Selection;
		Selection.Actors = SelectedActors;

		const FGridPlacementOptions Options = Menu::BuildPlacementOptions(Selection);

		TestEqual(TEXT("Anchor location"), Options.AnchorTransform.GetLocation(), SelectedActors[0]->GetActorLocation(), PositionTolerance);
		TestTrue(TEXT("Anchor rotation"), Options.AnchorTransform.Rotator().Equals(SelectedActors[0]->GetActorRotation(), PositionTolerance));
		TestTrue(TEXT("Anchor scale"), Options.AnchorTransform.GetScale3D().Equals(SelectedActors[0]->GetActorScale3D(), PositionTolerance));
		TestFalse(TEXT("Anchor is not the manual transform"), Options.AnchorTransform.GetLocation().Equals(FVector(1.0, 2.0, 3.0), PositionTolerance));
	}

	// An invalid first actor is not an anchor, so the manual transform has to be used again.
	{
		Settings.bUseFirstSelectedActor = true;

		FPlacementSelection Selection;
		Selection.Actors = { nullptr, SelectedActors[0] };

		const FGridPlacementOptions Options = Menu::BuildPlacementOptions(Selection);

		TestEqual(TEXT("Invalid anchor falls back"), Options.AnchorTransform.GetLocation(), FVector(1.0, 2.0, 3.0), PositionTolerance);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementMenuExecutionRulesTest,
	"GridPlacementTool.Menu.ExecutionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** The rules that decide whether the Place button is enabled, and which mode it runs in. */
bool FGridPlacementMenuExecutionRulesTest::RunTest(const FString& Parameters)
{
	FScopedSettingsOverride SettingsOverride;
	UGridPlacementToolSettings& Settings = SettingsOverride.Get();

	FScopedTestWorld TestWorld(*this);
	UWorld* World = TestWorld.Get();
	if (!World)
	{
		return false;
	}

	const TArray<AActor*> SpawnedActors = SpawnBoxActors(*World, { FVector(10.0, 10.0, 10.0), FVector(20.0, 10.0, 10.0) });
	const TArray<AActor*> OneActor = { SpawnedActors[0] };
	const TArray<UObject*> OneAsset = { MakeAsset() };

	{
		const FPlacementSelection Empty;
		TestFalse(TEXT("No selection has no anchor"), Menu::HasAnchorActor(Empty));

		FPlacementSelection NullFirst;
		NullFirst.Actors = { nullptr, SpawnedActors[0] };
		TestFalse(TEXT("Invalid first actor is not an anchor"), Menu::HasAnchorActor(NullFirst));

		FPlacementSelection WithActor;
		WithActor.Actors = OneActor;
		TestTrue(TEXT("First actor is the anchor"), Menu::HasAnchorActor(WithActor));
	}

	// Without the anchor setting, anything selected can be placed.
	{
		Settings.bUseFirstSelectedActor = false;

		const FPlacementSelection Empty;
		TestFalse(TEXT("Nothing selected"), Menu::CanExecutePlacement(Empty));

		FPlacementSelection SingleActorSelection;
		SingleActorSelection.Actors = OneActor;
		TestTrue(TEXT("A single actor"), Menu::CanExecutePlacement(SingleActorSelection));

		FPlacementSelection AssetSelection;
		AssetSelection.Assets = OneAsset;
		TestTrue(TEXT("A single asset"), Menu::CanExecutePlacement(AssetSelection));
	}

	// With the anchor setting, one actor is the anchor only: it needs something else to arrange around it.
	{
		Settings.bUseFirstSelectedActor = true;

		FPlacementSelection SingleActorSelection;
		SingleActorSelection.Actors = OneActor;
		TestFalse(TEXT("An anchor alone is not enough"), Menu::CanExecutePlacement(SingleActorSelection));

		FPlacementSelection AssetSelection;
		AssetSelection.Assets = OneAsset;
		TestFalse(TEXT("Assets without an anchor"), Menu::CanExecutePlacement(AssetSelection));

		FPlacementSelection AnchorWithAsset;
		AnchorWithAsset.Actors = OneActor;
		AnchorWithAsset.Assets = OneAsset;
		TestTrue(TEXT("An anchor plus assets"), Menu::CanExecutePlacement(AnchorWithAsset));

		FPlacementSelection TwoActors;
		TwoActors.Actors = SpawnedActors;
		TestTrue(TEXT("Two actors"), Menu::CanExecutePlacement(TwoActors));
	}

	// Scene actors win over Content Browser assets, except when a single actor is only the anchor.
	{
		FPlacementSelection StageAndAssets;
		StageAndAssets.Actors = SpawnedActors;
		StageAndAssets.Assets = OneAsset;
		TestTrue(TEXT("Stage win over assets"), Menu::ResolvePlacementMode(StageAndAssets) == Menu::EPlacementMode::Actors);

		FPlacementSelection SingleActorSelection;
		SingleActorSelection.Actors = OneActor;
		TestTrue(TEXT("A single actor is placed as an actor"), Menu::ResolvePlacementMode(SingleActorSelection) == Menu::EPlacementMode::Actors);

		FPlacementSelection AnchorWithAsset;
		AnchorWithAsset.Actors = OneActor;
		AnchorWithAsset.Assets = OneAsset;
		TestTrue(TEXT("Assets when the actor only anchors"), Menu::ResolvePlacementMode(AnchorWithAsset) == Menu::EPlacementMode::Assets);

		FPlacementSelection AssetSelection;
		AssetSelection.Assets = OneAsset;
		TestTrue(TEXT("Assets only"), Menu::ResolvePlacementMode(AssetSelection) == Menu::EPlacementMode::Assets);

		FPlacementSelection Empty;
		TestTrue(TEXT("Nothing to place"), Menu::ResolvePlacementMode(Empty) == Menu::EPlacementMode::None);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementMenuPlaceActorsTest,
	"GridPlacementTool.Menu.PlaceActorsOnGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Placing real editor actors applies the layout transforms and mirrors a reorder in the selection. */
bool FGridPlacementMenuPlaceActorsTest::RunTest(const FString& Parameters)
{
	FScopedSettingsOverride SettingsOverride;
	UGridPlacementToolSettings& Settings = SettingsOverride.Get();

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), EditorWorld))
	{
		return false;
	}

	FScopedEditorActors ActorGuard(*EditorWorld);

	// Bounds areas are 400, 800 and 1200, so descending sorting reverses the order.
	TArray<AActor*> Actors;
	Actors.Add(ActorGuard.AddBox(FVector(10.0, 10.0, 10.0)));
	Actors.Add(ActorGuard.AddBox(FVector(20.0, 10.0, 10.0)));
	Actors.Add(ActorGuard.AddBox(FVector(30.0, 10.0, 10.0)));
	for (AActor* Actor : Actors)
	{
		if (!TestNotNull(TEXT("Spawned actor"), Actor))
		{
			return false;
		}
	}

	Settings.Spacing = FVector2D(100.0, 200.0);
	Settings.Columns = 2;
	Settings.InitialTransform = FEulerTransform();
	Settings.bUseFirstSelectedActor = false;
	Settings.bAlignByObjectCenter = false;
	Settings.bSizeBasedSpacing = false;
	Settings.bFlowLayout = false;

	// With sorting on, the layout reorders its items and the editor selection has to follow that order.
	{
		Settings.bEnableSorting = true;
		Settings.bDescendingOrder = true;

		FPlacementSelection Selection;
		Selection.Actors = Actors;

		Menu::PlaceActorsOnGrid(Selection);

		const TArray<AActor*> ExpectedOrder = { Actors[2], Actors[1], Actors[0] };
		TestTrue(TEXT("Result order is descending"), Selection.Actors == ExpectedOrder);
		ExpectActorLocations(
			*this,
			TEXT("Sorted placement"),
			Selection.Actors,
			{ FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(100.0, 0.0, 0.0) });
		TestTrue(TEXT("Editor selection mirrors the order"), GetEditorSelection() == ExpectedOrder);
	}

	// Without sorting there is nothing to reselect, so the editor selection must be left alone.
	{
		Settings.bEnableSorting = false;

		GEditor->SelectNone(false, true);
		GEditor->SelectActor(Actors[0], true, true);

		FPlacementSelection Selection;
		Selection.Actors = Actors;

		Menu::PlaceActorsOnGrid(Selection);

		const TArray<AActor*> ExpectedSelection = { Actors[0] };
		TestTrue(TEXT("Order is untouched"), Selection.Actors == Actors);
		ExpectActorLocations(
			*this,
			TEXT("Unsorted placement"),
			Selection.Actors,
			{ FVector(0.0, 0.0, 0.0), FVector(0.0, 200.0, 0.0), FVector(100.0, 0.0, 0.0) });
		TestTrue(TEXT("Editor selection is untouched"), GetEditorSelection() == ExpectedSelection);
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementMenuSpawnAssetsTest,
	"GridPlacementTool.Menu.SpawnAssetsOnGrid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Spawning assets creates real actors, places them on the grid and selects them. */
bool FGridPlacementMenuSpawnAssetsTest::RunTest(const FString& Parameters)
{
	FScopedSettingsOverride SettingsOverride;
	UGridPlacementToolSettings& Settings = SettingsOverride.Get();

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), EditorWorld))
	{
		return false;
	}

	// An engine primitive keeps the test free of project content and has a centered pivot.
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (!TestNotNull(TEXT("Cube mesh"), CubeMesh))
	{
		return false;
	}

	FScopedEditorActors ActorGuard(*EditorWorld);

	Settings.Spacing = FVector2D(100.0, 200.0);
	Settings.Columns = 2;
	Settings.InitialTransform = FEulerTransform();
	Settings.bUseFirstSelectedActor = false;
	Settings.bAlignByObjectCenter = false;
	Settings.bSizeBasedSpacing = false;
	Settings.bFlowLayout = false;
	Settings.bEnableSorting = false;

	FPlacementSelection Selection;
	Selection.Assets = { CubeMesh, CubeMesh };

	Menu::SpawnAssetsOnGrid(Selection);

	const TArray<AActor*> SpawnedActors = GetEditorSelection();
	ActorGuard.Track(SpawnedActors);

	TestEqual(TEXT("Spawned actor count"), SpawnedActors.Num(), 2);
	if (SpawnedActors.Num() != 2)
	{
		return false;
	}

	// The first item always lands on the anchor; the second one is one column spacing to the right.
	TestEqual(TEXT("First item on the anchor"), SpawnedActors[0]->GetActorLocation(), FVector::ZeroVector, PositionTolerance);
	TestEqual(
		TEXT("Second item offset"),
		SpawnedActors[1]->GetActorLocation() - SpawnedActors[0]->GetActorLocation(),
		FVector(0.0, 200.0, 0.0),
		PositionTolerance);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGridPlacementMenuUndoRedoTest,
	"GridPlacementTool.Menu.UndoRedo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Placing real editor actors creates an editor transaction and can be undone via UndoTransaction. */
bool FGridPlacementMenuUndoRedoTest::RunTest(const FString& Parameters)
{
	FScopedSettingsOverride SettingsOverride;
	UGridPlacementToolSettings& Settings = SettingsOverride.Get();

	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!TestNotNull(TEXT("Editor world"), EditorWorld))
	{
		return false;
	}

	FScopedEditorActors ActorGuard(*EditorWorld);

	TArray<AActor*> Actors;
	Actors.Add(ActorGuard.AddBox(FVector(10.0, 10.0, 10.0)));
	Actors.Add(ActorGuard.AddBox(FVector(20.0, 10.0, 10.0)));
	for (AActor* Actor : Actors)
	{
		if (!TestNotNull(TEXT("Spawned actor"), Actor))
		{
			return false;
		}
	}

	const FVector InitialLocation0(1000.0, 500.0, 0.0);
	const FVector InitialLocation1(2000.0, 500.0, 0.0);
	Actors[0]->SetActorLocation(InitialLocation0);
	Actors[1]->SetActorLocation(InitialLocation1);

	Settings.Spacing = FVector2D(100.0, 200.0);
	Settings.Columns = 2;
	Settings.InitialTransform = FEulerTransform();
	Settings.bUseFirstSelectedActor = false;
	Settings.bAlignByObjectCenter = false;
	Settings.bSizeBasedSpacing = false;
	Settings.bFlowLayout = false;
	Settings.bEnableSorting = false;

	FPlacementSelection Selection;
	Selection.Actors = Actors;

	Menu::PlaceActorsOnGrid(Selection);

	// Verify placed positions
	TestEqual(TEXT("Actor 0 placed location"), Actors[0]->GetActorLocation(), FVector(0.0, 0.0, 0.0), PositionTolerance);
	TestEqual(TEXT("Actor 1 placed location"), Actors[1]->GetActorLocation(), FVector(0.0, 200.0, 0.0), PositionTolerance);

	// Undo the transaction
	TestTrue(TEXT("Undo transaction succeeded"), GEditor->UndoTransaction());
	TestEqual(TEXT("Actor 0 restored location after undo"), Actors[0]->GetActorLocation(), InitialLocation0, PositionTolerance);
	TestEqual(TEXT("Actor 1 restored location after undo"), Actors[1]->GetActorLocation(), InitialLocation1, PositionTolerance);

	// Redo the transaction
	TestTrue(TEXT("Redo transaction succeeded"), GEditor->RedoTransaction());
	TestEqual(TEXT("Actor 0 redone location"), Actors[0]->GetActorLocation(), FVector(0.0, 0.0, 0.0), PositionTolerance);
	TestEqual(TEXT("Actor 1 redone location"), Actors[1]->GetActorLocation(), FVector(0.0, 200.0, 0.0), PositionTolerance);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

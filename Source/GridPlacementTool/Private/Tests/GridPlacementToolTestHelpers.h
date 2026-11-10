// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "GridPlacementToolLayout.h"
#include "GridPlacementToolSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

/**
 * Shared fixtures for the GridPlacementTool automation tests.
 *
 * The tool itself is a thin layer on top of UGridPlacementToolSettings, so every test that goes
 * through it has to write to the config-backed defaults. FScopedSettingsOverride keeps those writes
 * local to a test, and FScopedTestWorld keeps the world setup out of the test bodies.
 */
namespace GridPlacementTool::Test
{
	inline constexpr float PositionTolerance = 0.001f;

	/**
	 * Spawns an actor whose only colliding geometry is a box, so GetActorBounds is fully deterministic.
	 * A separate root component keeps the actor pivot at the origin while the box carries an arbitrary
	 * offset, which is what the center-alignment compensation has to deal with.
	 */
	inline AActor* SpawnBoxActor(
		UWorld& World,
		const FVector& BoxExtent,
		const FVector& PivotOffset = FVector::ZeroVector,
		const FVector& Scale = FVector::OneVector,
		bool bTransient = false)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transactional;
		if (bTransient)
		{
			SpawnParameters.ObjectFlags |= RF_Transient;
		}

		AActor* Actor = World.SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		if (!Actor)
		{
			return nullptr;
		}

		USceneComponent* Root = NewObject<USceneComponent>(Actor, TEXT("Root"), RF_Transactional);
		Actor->SetRootComponent(Root);
		Root->RegisterComponent();

		UBoxComponent* Box = NewObject<UBoxComponent>(Actor, TEXT("TestBox"), RF_Transactional);
		Box->SetupAttachment(Root);
		Box->SetBoxExtent(BoxExtent);
		Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Box->SetRelativeLocation(PivotOffset);
		Box->RegisterComponent();

		Actor->SetActorScale3D(Scale);
		return Actor;
	}

	inline TArray<AActor*> SpawnBoxActors(
		UWorld& World,
		const TArray<FVector>& BoxExtents,
		const FVector& PivotOffset = FVector::ZeroVector,
		const FVector& Scale = FVector::OneVector,
		bool bTransient = false)
	{
		TArray<AActor*> Actors;
		Actors.Reserve(BoxExtents.Num());
		for (const FVector& BoxExtent : BoxExtents)
		{
			Actors.Add(SpawnBoxActor(World, BoxExtent, PivotOffset, Scale, bTransient));
		}
		return Actors;
	}

	/** Throwaway game world that is destroyed once the object goes out of scope. */
	struct FScopedTestWorld
	{
		explicit FScopedTestWorld(FAutomationTestBase& Test)
		{
			if (WorldWrapper.CreateTestWorld(EWorldType::Game))
			{
				World = WorldWrapper.GetTestWorld();
			}
			Test.TestNotNull(TEXT("Test world"), World);
		}

		UWorld* Get() const { return World; }

	private:
		FTestWorldWrapper WorldWrapper;
		UWorld* World = nullptr;
	};

	/**
	 * Snapshots the tool settings and restores them on destruction, so a test can drive the real
	 * config-backed defaults without leaking the changes into the editor's user settings.
	 * Nothing here calls SaveConfig, so the ini file is never rewritten.
	 */
	class FScopedSettingsOverride
	{
	public:
		FScopedSettingsOverride()
		{
			const UGridPlacementToolSettings& Settings = *GetDefault<UGridPlacementToolSettings>();
			Spacing = Settings.Spacing;
			Columns = Settings.Columns;
			InitialTransform = Settings.InitialTransform;
			bUseFirstSelectedActor = Settings.bUseFirstSelectedActor;
			bAlignByObjectCenter = Settings.bAlignByObjectCenter;
			bEnableSorting = Settings.bEnableSorting;
			bDescendingOrder = Settings.bDescendingOrder;
			bSizeBasedSpacing = Settings.bSizeBasedSpacing;
			bFlowLayout = Settings.bFlowLayout;
		}

		~FScopedSettingsOverride()
		{
			UGridPlacementToolSettings* Settings = GetMutableDefault<UGridPlacementToolSettings>();
			Settings->Spacing = Spacing;
			Settings->Columns = Columns;
			Settings->InitialTransform = InitialTransform;
			Settings->bUseFirstSelectedActor = bUseFirstSelectedActor;
			Settings->bAlignByObjectCenter = bAlignByObjectCenter;
			Settings->bEnableSorting = bEnableSorting;
			Settings->bDescendingOrder = bDescendingOrder;
			Settings->bSizeBasedSpacing = bSizeBasedSpacing;
			Settings->bFlowLayout = bFlowLayout;
		}

		/** Settings a test may write to freely; the values are restored on destruction. */
		UGridPlacementToolSettings& Get() const
		{
			return *GetMutableDefault<UGridPlacementToolSettings>();
		}

	private:
		FVector2D Spacing;
		int32 Columns = 1;
		FEulerTransform InitialTransform;
		bool bUseFirstSelectedActor = false;
		bool bAlignByObjectCenter = false;
		bool bEnableSorting = false;
		bool bDescendingOrder = false;
		bool bSizeBasedSpacing = false;
		bool bFlowLayout = false;
	};

	/** Asserts that a layout result places its items at the expected locations, in order. */
	inline void ExpectLocations(
		FAutomationTestBase& Test,
		const FString& Label,
		const FGridPlacementResult& Result,
		const TArray<FVector>& Expected)
	{
		Test.TestEqual(Label + TEXT(": item count"), Result.Transforms.Num(), Expected.Num());
		for (int32 Index = 0; Index < FMath::Min(Result.Transforms.Num(), Expected.Num()); ++Index)
		{
			Test.TestEqual(
				FString::Printf(TEXT("%s: item %d"), *Label, Index),
				Result.Transforms[Index].GetLocation(),
				Expected[Index],
				PositionTolerance);
		}
	}
}

#endif // WITH_DEV_AUTOMATION_TESTS

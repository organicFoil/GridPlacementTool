// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#pragma once

#include "GridPlacementToolLayout.h"
#include "GridPlacementToolSettings.h"
#include "GridPlacementToolTypes.h"
#include "SAdvancedTransformInputBox.h"

class UToolMenu;

namespace GridPlacementTool::Menu
{
	extern const FName GridPlacementToolMenuName;

	enum class EPlacementMode : uint8
	{
		None,
		Actors,
		Assets
	};

	const UGridPlacementToolSettings& GetSettings();

	FPlacementSelection CollectSelection();
	bool HasAnchorActor(const FPlacementSelection& Selection);
	bool CanExecutePlacement(const FPlacementSelection& Selection);
	EPlacementMode ResolvePlacementMode(FPlacementSelection& Selection);
	FGridPlacementOptions BuildPlacementOptions(const FPlacementSelection& Selection);
	void ReselectActors(const TArray<AActor*>& Actors);
	void PlaceActorsOnGrid(FPlacementSelection& Selection);
	void SpawnAssetsOnGrid(const FPlacementSelection& Selection);
	bool HasSelectedActor();

	FText BuildRunButtonHintText();
	FOptionalSize GetMainWidgetWidth();
	void BuildSettingsSection(UToolMenu& Menu);
	void BuildAnchorSection(UToolMenu& Menu);
	void BuildSortingSection(UToolMenu& Menu);
	void BuildSizeBasedSpacingSection(UToolMenu& Menu);
	void BuildRunSection(UToolMenu& Menu);
	void BuildMenuIfNeeded();

	bool IsRunButtonEnabled();
	FReply OnRunClicked();
	FReply OnCopyTransformClicked();
	bool IsResetTransformEnabled();
	FReply OnResetTransformClicked();

	namespace Spacing
	{
		TOptional<FVector2D> GetValue();
		void OnChanged(double Spacing, EAxis::Type Axis);
		void OnCommitted(double Spacing, ETextCommit::Type CommitType, EAxis::Type Axis);
	}

	namespace Columns
	{
		int32 GetValue();
		void OnChanged(int32 Columns);
		void OnCommitted(int32 Columns, ETextCommit::Type CommitType);
	}

	namespace InitialTransform
	{
		void ToggleUseFirstSelectedActor();
		bool IsUseFirstSelectedActor();
		EVisibility GetTransformVisibility();

		TOptional<FEulerTransform> GetTransform();
		void SetTransform(const FEulerTransform& Transform, bool bSaveConfig);

		void OnNumericValueChanged(
			ESlateTransformComponent::Type Component,
			ESlateRotationRepresentation::Type RotationRepresentation,
			ESlateTransformSubComponent::Type SubComponent,
			double Value);

		void OnNumericValueCommitted(
			ESlateTransformComponent::Type Component,
			ESlateRotationRepresentation::Type RotationRepresentation,
			ESlateTransformSubComponent::Type SubComponent,
			double Value,
			ETextCommit::Type CommitType);
	}

	namespace ObjectAlignment
	{
		void ToggleAlignByObjectCenter();
		bool IsAlignByObjectCenter();
	}

	namespace Sorting
	{
		void ToggleEnableSorting();
		bool IsSortingEnabled();
		EVisibility GetSortingOptionsVisibility();

		void ToggleDescendingOrder();
		bool IsDescendingOrder();
	}

	namespace SizeBasedSpacing
	{
		void ToggleSizeBasedSpacing();
		bool IsSizeBasedSpacing();
		EVisibility GetSizeBasedSpacingOptionsVisibility();

		void ToggleFlowLayout();
		bool IsFlowLayout();
	}

	struct FButtonConstructor
	{
		FOnClicked OnClicked;
		FName Icon;
		FText ButtonText;
		FText TooltipText;
		TAttribute<bool> IsEnabled = true;
		float MinWidth = 160.f;

		TSharedRef<SWidget> ConstructWidget() const;
	};
}

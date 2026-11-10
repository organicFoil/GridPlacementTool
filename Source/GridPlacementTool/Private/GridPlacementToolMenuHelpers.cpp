// Copyright Aleksandr Yakshigulov. All Rights Reserved.

#include "GridPlacementToolMenuHelpers.h"

#include "Editor.h"
#include "EditorUtilityLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "GridPlacementTool.h"
#include "GridPlacementToolCommands.h"
#include "GridPlacementToolSettings.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "ToolMenus.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SVectorInputBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "GridPlacementToolMenuHelpers"

using SVector2DInputBox = SNumericVectorInputBox<FVector2D::FReal, FVector2D, 2>;

namespace GridPlacementTool::Menu
{
	extern const FName GridPlacementToolMenuName(TEXT("UnrealEd.PlayWorldCommands.GridPlacementToolMenu"));

	const UGridPlacementToolSettings& GetSettings()
	{
		return *GetDefault<UGridPlacementToolSettings>();
	}

	FPlacementSelection CollectSelection()
	{
		FPlacementSelection Selection;
		Selection.Actors = UEditorUtilityLibrary::GetSelectionSet();
		Selection.Assets = UEditorUtilityLibrary::GetSelectedAssets();
		return Selection;
	}

	bool HasAnchorActor(const FPlacementSelection& Selection)
	{
		return Selection.Actors.IsValidIndex(0) && Selection.Actors[0] != nullptr;
	}

	bool CanExecutePlacement(const FPlacementSelection& Selection)
	{
		const bool bActorsSelected = !Selection.Actors.IsEmpty();
		const bool bAssetsSelected = !Selection.Assets.IsEmpty();
		const bool bMoreThanOneActorSelected = Selection.Actors.Num() > 1;

		if (GetSettings().bUseFirstSelectedActor)
		{
			return bMoreThanOneActorSelected || (HasAnchorActor(Selection) && bAssetsSelected);
		}

		return bActorsSelected || bAssetsSelected;
	}

	EPlacementMode ResolvePlacementMode(FPlacementSelection& Selection)
	{
		// The priority is the number of selected actors on the stage. If there is more than one, then the mode will be selected for them.
		if (Selection.Actors.Num() > 1 || Selection.Assets.IsEmpty())
		{
			return Selection.Actors.IsEmpty() ? EPlacementMode::None : EPlacementMode::Actors;
		}

		return Selection.Assets.IsEmpty() ? EPlacementMode::None : EPlacementMode::Assets;
	}

	FGridPlacementOptions BuildPlacementOptions(const FPlacementSelection& Selection)
	{
		const UGridPlacementToolSettings& Settings = GetSettings();
		FGridPlacementOptions Options = FGridPlacementOptions::FromSettings(Settings);

		// The anchor must be resolved from the original selection order, before the layout sort runs.
		if (Settings.bUseFirstSelectedActor && HasAnchorActor(Selection))
		{
			Options.AnchorTransform = Selection.Actors[0]->GetActorTransform();
		}

		return Options;
	}

	void ReselectActors(const TArray<AActor*>& Actors)
	{
		GEditor->SelectNone(false, true);
		for (int32 Index = 0; Index < Actors.Num(); ++Index)
		{
			// Update the editor only on the last iteration
			GEditor->SelectActor(Actors[Index], true, Index == Actors.Num() - 1);
		}
	}

	void PlaceActorsOnGrid(FPlacementSelection& Selection)
	{
		const FScopedTransaction Transaction(LOCTEXT("PlaceActorsOnGrid", "Place Actors on Grid"));

		FGridPlacementResult Result = GridPlacement::Layout::Solve(Selection.Actors, BuildPlacementOptions(Selection));

		for (int32 Index = 0; Index < Result.Actors.Num(); ++Index)
		{
			AActor* Actor = Result.Actors[Index];
			if (IsValid(Actor))
			{
				Actor->Modify();
				Actor->SetActorTransform(Result.Transforms[Index]);
			}
		}

		// The layout may reorder the items (sorting); keep the selection in sync with it.
		Selection.Actors = MoveTemp(Result.Actors);
		if (Result.bReordered)
		{
			ReselectActors(Selection.Actors);
		}
	}

	void SpawnAssetsOnGrid(const FPlacementSelection& Selection)
	{
		const FScopedTransaction Transaction(LOCTEXT("SpawnAssetsOnGrid", "Spawn Assets on Grid"));

		UEditorActorSubsystem* EditorActorSubsystem = GEditor->GetEditorSubsystem<UEditorActorSubsystem>();

		const TArray<UObject*>& Assets = Selection.Assets;
		TArray<AActor*> SpawnedActors;

		GEditor->SelectNone(false, true);

		// Spawn all assets via FScopedSlowTask progress bar
		FScopedSlowTask SlowTask(
			100.0f,
			LOCTEXT("SpawnAssetsTask", "Spawning assets...")
		);
		SlowTask.MakeDialog(true);

		for (int32 Index = 0; Index < Assets.Num(); ++Index)
		{
			if (SlowTask.ShouldCancel()) 
				break;

			SlowTask.EnterProgressFrame(100.0f / Assets.Num());

			UObject* Asset = Assets[Index];
			if (!IsValid(Asset)) 
				continue;

			if (AActor* NewActor = EditorActorSubsystem->SpawnActorFromObject(Asset, FVector(), FRotator()))
			{
				SpawnedActors.Add(NewActor);
			}
		}

		// Process all spawned actors and set their new transform, then select them
		const FGridPlacementResult Result = GridPlacement::Layout::Solve(SpawnedActors, BuildPlacementOptions(Selection));

		for (int32 Index = 0; Index < Result.Actors.Num(); ++Index)
		{
			AActor* SpawnedActor = Result.Actors[Index];
			if (IsValid(SpawnedActor))
			{
				SpawnedActor->Modify();
				SpawnedActor->SetActorTransform(Result.Transforms[Index]);
			}
		}
		ReselectActors(Result.Actors);
	}

	bool HasSelectedActor()
	{
		return !UEditorUtilityLibrary::GetSelectionSet().IsEmpty();
	}

	FText BuildRunButtonHintText()
	{
		const FPlacementSelection Selection = CollectSelection();
		const int32 ActorsNum = Selection.Actors.Num();
		const int32 AssetsNum = Selection.Assets.Num();

		if (ActorsNum == 0 && AssetsNum == 0)
		{
			return LOCTEXT("RunButtonHint_SelectActorsAssets", "Select more actors or assets");
		}

		if (GetSettings().bUseFirstSelectedActor)
		{
			if (ActorsNum == 0 || (ActorsNum <= 1 && AssetsNum == 0))
			{
				return LOCTEXT("RunButtonHint_SelectActors", "Select more actors");
			}
		}

		if (ActorsNum > 1 || AssetsNum == 0)
		{
			return FText::Format(
				LOCTEXT("RunButtonHint_PlaceActors", "Place {Count} scene actors"),
				FFormatNamedArguments{{TEXT("Count"), ActorsNum}});
		}

		return FText::Format(
			LOCTEXT("RunButtonHint_PlaceAssets", "Place {Count} assets from Content Browser"),
			FFormatNamedArguments{{TEXT("Count"), AssetsNum}});
	}

	FOptionalSize GetMainWidgetWidth()
	{
		return 200.f;
	}

	void BuildSettingsSection(UToolMenu& Menu)
	{
		FToolMenuSection& SettingsSection = Menu.AddSection("SettingsSection", LOCTEXT("SettingsSection", "Settings"));

		TSharedRef<SWidget> SpacingWidget =
			SNew(SBox)
			.MinDesiredWidth_Static(&GetMainWidgetWidth) // Here the minimum width of the entire widget is hardcoded, using a wrapper Box (stretches the entire widget).
			[
				SNew(SVector2DInputBox)
				.bColorAxisLabels(true)
				.AllowSpin(true)
				.Vector_Static(&Spacing::GetValue)
				.OnXChanged_Static(&Spacing::OnChanged, EAxis::X)
				.OnYChanged_Static(&Spacing::OnChanged, EAxis::Y)
				.OnXCommitted_Static(&Spacing::OnCommitted, EAxis::X)
				.OnYCommitted_Static(&Spacing::OnCommitted, EAxis::Y)
				.ToolTipText(LOCTEXT("SpacingTooltip", "Spacing between grid items (X: Forward / Row spacing, Y: Right / Column spacing)"))
			];
		SettingsSection.AddEntry(FToolMenuEntry::InitWidget("Spacing", SpacingWidget, LOCTEXT("Spacing", "Spacing")));

		TSharedRef<SWidget> ColumnsWidget = SNew(SSpinBox<int32>)
			.MinValue(1)
			.MaxValue(MAX_int32)
			.MinSliderValue(1)
			.MaxSliderValue(100)
			.Delta(1)
			.Value_Static(&Columns::GetValue)
			.OnValueChanged_Static(&Columns::OnChanged)
			.OnValueCommitted_Static(&Columns::OnCommitted)
			.ToolTipText(LOCTEXT("ColumnsTooltip", "How many elements will be in the grid along its relative X axis."));
		SettingsSection.AddEntry(FToolMenuEntry::InitWidget("Columns", ColumnsWidget, LOCTEXT("Columns", "Columns")));
	}

	void BuildAnchorSection(UToolMenu& Menu)
	{
		FToolMenuSection& AnchorSection = Menu.AddSection("AnchorSection", LOCTEXT("AnchorSection", "Anchor"));

		const FGridPlacementToolCommands& Commands = FGridPlacementToolCommands::Get();
		AnchorSection.AddMenuEntry(Commands.AlignByObjectCenter);
		AnchorSection.AddMenuEntry(Commands.UseFirstSelectedActor);

		FButtonConstructor CopyTransformButton;
		CopyTransformButton.OnClicked = FOnClicked::CreateStatic(&OnCopyTransformClicked);
		CopyTransformButton.IsEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&HasSelectedActor));
		CopyTransformButton.Icon = "ClassIcon.SceneComponent";
		CopyTransformButton.ButtonText = LOCTEXT("CopyTransformButtonText", "Copy transform");
		CopyTransformButton.TooltipText = LOCTEXT("CopyTransformButtonTooltip", "Saves the transform of the selected actor into tool.");
		CopyTransformButton.MinWidth = 120.f;

		FButtonConstructor ResetButton;
		ResetButton.OnClicked = FOnClicked::CreateStatic(&OnResetTransformClicked);
		ResetButton.IsEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&IsResetTransformEnabled));
		ResetButton.ButtonText = LOCTEXT("ResetTransformButtonText", "Reset");
		ResetButton.TooltipText = LOCTEXT("ResetTransformTooltip", "Resets the world transform to identity (zero position, no rotation, scale 1).");
		ResetButton.MinWidth = 60.f;

		TSharedRef<SWidget> InitialTransformWidget = SNew(SVerticalBox)
			.Visibility_Static(&InitialTransform::GetTransformVisibility)
			+ SVerticalBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(18.f, 6.f, 18.f, 0.f)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text(LOCTEXT("InitialTransformHint", "World transform"))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(18.f, 4.f, 18.f, 4.f)
				[
					SNew(SAdvancedTransformInputBox<FEulerTransform>)
					.ConstructScale(false)
					.Transform_Static(&InitialTransform::GetTransform)
					.OnNumericValueChanged_Static(&InitialTransform::OnNumericValueChanged)
					.OnNumericValueCommitted_Static(&InitialTransform::OnNumericValueCommitted)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					CopyTransformButton.ConstructWidget()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.f, 0.f, 0.f, 0.f)
				[
					ResetButton.ConstructWidget()
				]
			];
		AnchorSection.AddEntry(FToolMenuEntry::InitWidget("InitialTransform", InitialTransformWidget, FText::GetEmpty()));
	}

	void BuildSortingSection(UToolMenu& Menu)
	{
		const FGridPlacementToolCommands& Commands = FGridPlacementToolCommands::Get();

		FToolMenuSection& SortingSection = Menu.AddSection("SortingSection", LOCTEXT("SortingSection", "Sorting"));
		SortingSection.AddMenuEntry(Commands.EnableSorting);

		FToolMenuSection& SortingSubSection = Menu.AddSection("SortingSubSection", FText::GetEmpty());
		SortingSubSection.Visibility = &Sorting::GetSortingOptionsVisibility;
		SortingSubSection.AddMenuEntry(Commands.DescendingOrder);
	}

	void BuildSizeBasedSpacingSection(UToolMenu& Menu)
	{
		const FGridPlacementToolCommands& Commands = FGridPlacementToolCommands::Get();

		FToolMenuSection& SizeBasedSpacingSection = Menu.AddSection("SizeBasedSpacingSection", LOCTEXT("SizeBasedSpacingSection", "Size-based spacing"));
		SizeBasedSpacingSection.AddMenuEntry(Commands.SizeBasedSpacing);

		FToolMenuSection& SizeBasedSpacingSubSection = Menu.AddSection("SizeBasedSpacingSubSection", FText::GetEmpty());
		SizeBasedSpacingSubSection.Visibility = &SizeBasedSpacing::GetSizeBasedSpacingOptionsVisibility;
		SizeBasedSpacingSubSection.AddMenuEntry(Commands.FlowLayout);
	}

	void BuildRunSection(UToolMenu& Menu)
	{
		FToolMenuSection& RunSection = Menu.AddSection("RunSection", FText::GetEmpty());
		RunSection.AddSeparator("RunSectionSeparator");

		TSharedRef<SWidget> HintWidget = SNew(SBox)
			.Padding(FMargin(16.0f, 3.0f))
			[
				SNew(STextBlock)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				.Text_Static(&BuildRunButtonHintText)
				.WrapTextAt(250)
			];
		RunSection.AddEntry(FToolMenuEntry::InitWidget("RunButtonHint", HintWidget, FText::GetEmpty()));

		FButtonConstructor RunButton;
		RunButton.OnClicked = FOnClicked::CreateStatic(&OnRunClicked);
		RunButton.IsEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&IsRunButtonEnabled));
		RunButton.Icon = "EditorViewport.LocationGridSnap";
		RunButton.ButtonText = LOCTEXT("RunButtonText", "Place");
		RunButton.TooltipText = LOCTEXT("RunButtonTooltip", "The tool can place selected actors in the scene or selected assets in the Content Browser on a grid if the Anchor settings are configured.");
		RunSection.AddEntry(FToolMenuEntry::InitWidget("RunButton", RunButton.ConstructWidget(), FText::GetEmpty()));
	}

	void BuildMenuIfNeeded()
	{
		UToolMenus* ToolMenus = UToolMenus::Get();
		if (ToolMenus->IsMenuRegistered(GridPlacementToolMenuName))
		{
			return;
		}

		UToolMenu* Menu = ToolMenus->RegisterMenu(GridPlacementToolMenuName);
		BuildSettingsSection(*Menu);
		BuildAnchorSection(*Menu);
		BuildSortingSection(*Menu);
		BuildSizeBasedSpacingSection(*Menu);
		BuildRunSection(*Menu);
	}

	bool IsRunButtonEnabled()
	{
		return CanExecutePlacement(CollectSelection());
	}

	FReply OnRunClicked()
	{
		FPlacementSelection Selection = CollectSelection();
		if (!CanExecutePlacement(Selection))
		{
			return FReply::Handled();
		}

		switch (ResolvePlacementMode(Selection))
		{
		case EPlacementMode::Actors:
			PlaceActorsOnGrid(Selection);
			break;
		case EPlacementMode::Assets:
			SpawnAssetsOnGrid(Selection);
			break;
		default:
			return FReply::Handled();
		}

		// Updates the selection of actors on the stage
		GEditor->NoteSelectionChange();

		FSlateApplication::Get().DismissAllMenus();

		UE_LOG(LogGridPlacementTool, Log, TEXT("Tool task completed."));
		return FReply::Handled();
	}

	FReply OnCopyTransformClicked()
	{
		const FPlacementSelection Selection = CollectSelection();
		if (HasAnchorActor(Selection))
		{
			InitialTransform::SetTransform(FEulerTransform(Selection.Actors[0]->GetTransform()), true);
		}
		else
		{
			UE_LOG(LogGridPlacementTool, Warning, TEXT("OnCopyTransform: No actors selected."));
		}

		return FReply::Handled();
	}

	bool IsResetTransformEnabled()
	{
		return !GetSettings().InitialTransform.Equals(FEulerTransform::Identity);
	}

	FReply OnResetTransformClicked()
	{
		const FScopedTransaction Transaction(LOCTEXT("ResetInitialTransform", "Reset Initial Transform"));
		GetMutableDefault<UGridPlacementToolSettings>()->Modify();
		InitialTransform::SetTransform(FEulerTransform::Identity, true);
		return FReply::Handled();
	}

	template <typename MutatorType>
	void UpdateSettings(MutatorType&& Mutator, bool bSaveConfig)
	{
		UGridPlacementToolSettings* Settings = GetMutableDefault<UGridPlacementToolSettings>();
		Mutator(*Settings);
		if (bSaveConfig)
		{
			Settings->PostEditChange();
			Settings->SaveConfig();
		}
	}

	namespace Spacing
	{
		TOptional<FVector2D> GetValue()
		{
			return GetDefault<UGridPlacementToolSettings>()->Spacing;
		}

		void SetValue(double Spacing, EAxis::Type Axis, bool bSaveConfig)
		{
			UpdateSettings(
				[Spacing, Axis](UGridPlacementToolSettings& Settings)
				{
					switch (Axis)
					{
					case EAxis::X:
						Settings.Spacing.X = Spacing;
						break;
					case EAxis::Y:
						Settings.Spacing.Y = Spacing;
						break;
					default:
						break;
					}
				},
				bSaveConfig);
		}

		void OnChanged(double Spacing, EAxis::Type Axis)
		{
			SetValue(Spacing, Axis, false);
		}

		void OnCommitted(double Spacing, ETextCommit::Type CommitType, EAxis::Type Axis)
		{
			if (CommitType == ETextCommit::OnCleared)
			{
				return;
			}

			const FScopedTransaction Transaction(LOCTEXT("SetSpacing", "Set Grid Spacing"));
			GetMutableDefault<UGridPlacementToolSettings>()->Modify();
			SetValue(Spacing, Axis, true);
		}
	}

	namespace Columns
	{
		int32 GetValue()
		{
			return GetDefault<UGridPlacementToolSettings>()->Columns;
		}

		void SetValue(int32 Columns, bool bSaveConfig)
		{
			UpdateSettings(
				[Columns](UGridPlacementToolSettings& Settings)
				{
					Settings.Columns = FMath::Max(1, Columns);
				},
				bSaveConfig);
		}

		void OnChanged(int32 Columns)
		{
			SetValue(Columns, false);
		}

		void OnCommitted(int32 Columns, ETextCommit::Type /*CommitType*/)
		{
			SetValue(Columns, true);
		}
	}

	namespace InitialTransform
	{
		void ToggleUseFirstSelectedActor()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bUseFirstSelectedActor = !Settings.bUseFirstSelectedActor;
				},
				true);
		}

		bool IsUseFirstSelectedActor()
		{
			return GetDefault<UGridPlacementToolSettings>()->bUseFirstSelectedActor;
		}

		EVisibility GetTransformVisibility()
		{
			if (GetDefault<UGridPlacementToolSettings>()->bUseFirstSelectedActor)
			{
				return EVisibility::Collapsed;
			}
			return EVisibility::Visible;
		}

		TOptional<FEulerTransform> GetTransform()
		{
			return GetDefault<UGridPlacementToolSettings>()->InitialTransform;
		}

		void SetTransform(const FEulerTransform& Transform, bool bSaveConfig)
		{
			UpdateSettings(
				[Transform](UGridPlacementToolSettings& Settings)
				{
					Settings.InitialTransform = Transform;
				},
				bSaveConfig);
		}

		void SetLocationComponent(FEulerTransform& Transform, ESlateTransformSubComponent::Type SubComponent, double Value)
		{
			FVector Location = Transform.GetLocation();
			switch (SubComponent)
			{
			case ESlateTransformSubComponent::X:
				Location.X = Value;
				break;
			case ESlateTransformSubComponent::Y:
				Location.Y = Value;
				break;
			case ESlateTransformSubComponent::Z:
				Location.Z = Value;
				break;
			default:
				return;
			}

			Transform.SetLocation(Location);
		}

		void SetRotationComponent(FEulerTransform& Transform, ESlateTransformSubComponent::Type SubComponent, double Value)
		{
			FRotator Rotation = Transform.Rotator();
			switch (SubComponent)
			{
			case ESlateTransformSubComponent::Pitch:
			case ESlateTransformSubComponent::Y:
				Rotation.Pitch = Value;
				break;
			case ESlateTransformSubComponent::Yaw:
			case ESlateTransformSubComponent::Z:
				Rotation.Yaw = Value;
				break;
			case ESlateTransformSubComponent::Roll:
			case ESlateTransformSubComponent::X:
				Rotation.Roll = Value;
				break;
			default:
				return;
			}

			Transform = FEulerTransform(Rotation, Transform.GetLocation(), Transform.GetScale3D());
			Transform.NormalizeRotation();
		}

		void ApplyNumericValue(
			ESlateTransformComponent::Type Component,
			ESlateTransformSubComponent::Type SubComponent,
			double Value,
			bool bSaveConfig)
		{
			FEulerTransform Transform = GetDefault<UGridPlacementToolSettings>()->InitialTransform;
			switch (Component)
			{
			case ESlateTransformComponent::Location:
				SetLocationComponent(Transform, SubComponent, Value);
				break;
			case ESlateTransformComponent::Rotation:
				SetRotationComponent(Transform, SubComponent, Value);
				break;
			default:
				return;
			}

			SetTransform(Transform, bSaveConfig);
		}

		void OnNumericValueChanged(
			ESlateTransformComponent::Type Component,
			ESlateRotationRepresentation::Type /*RotationRepresentation*/,
			ESlateTransformSubComponent::Type SubComponent,
			double Value)
		{
			ApplyNumericValue(Component, SubComponent, Value, false);
		}

		void OnNumericValueCommitted(
			ESlateTransformComponent::Type Component,
			ESlateRotationRepresentation::Type /*RotationRepresentation*/,
			ESlateTransformSubComponent::Type SubComponent,
			double Value,
			ETextCommit::Type /*CommitType*/)
		{
			ApplyNumericValue(Component, SubComponent, Value, true);
		}
	}

	namespace ObjectAlignment
	{
		void ToggleAlignByObjectCenter()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bAlignByObjectCenter = !Settings.bAlignByObjectCenter;
				},
				true);
		}

		bool IsAlignByObjectCenter()
		{
			return GetDefault<UGridPlacementToolSettings>()->bAlignByObjectCenter;
		}
	}

	namespace Sorting
	{
		void ToggleEnableSorting()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bEnableSorting = !Settings.bEnableSorting;
				},
				true);
		}

		bool IsSortingEnabled()
		{
			return GetDefault<UGridPlacementToolSettings>()->bEnableSorting;
		}

		EVisibility GetSortingOptionsVisibility()
		{
			return IsSortingEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
		}

		void ToggleDescendingOrder()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bDescendingOrder = !Settings.bDescendingOrder;
				},
				true);
		}

		bool IsDescendingOrder()
		{
			return GetDefault<UGridPlacementToolSettings>()->bDescendingOrder;
		}
	}

	namespace SizeBasedSpacing
	{
		void ToggleSizeBasedSpacing()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bSizeBasedSpacing = !Settings.bSizeBasedSpacing;
				},
				true);
		}

		bool IsSizeBasedSpacing()
		{
			return GetDefault<UGridPlacementToolSettings>()->bSizeBasedSpacing;
		}

		EVisibility GetSizeBasedSpacingOptionsVisibility()
		{
			return IsSizeBasedSpacing() ? EVisibility::Visible : EVisibility::Collapsed;
		}

		void ToggleFlowLayout()
		{
			UpdateSettings(
				[](UGridPlacementToolSettings& Settings)
				{
					Settings.bFlowLayout = !Settings.bFlowLayout;
				},
				true);
		}

		bool IsFlowLayout()
		{
			return GetDefault<UGridPlacementToolSettings>()->bFlowLayout;
		}
	}

	TSharedRef<SWidget> FButtonConstructor::ConstructWidget() const
	{
		TSharedRef<SWidget> IconWidget = SNew(SBox);
		if (!Icon.IsNone())
		{
			IconWidget = SNew(SBox)
				.Padding(0.f, 0.f, 6.f, 0.f)
				[
					SNew(SImage)
					.Image(FAppStyle::GetBrush(Icon))
				];
		}

		return SNew(SBox)
			.HAlign(HAlign_Center)
			.Padding(0.0f, 2.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MinWidth(MinWidth)
				[
					SNew(SButton)
					.IsEnabled(IsEnabled)
					.ButtonColorAndOpacity(FLinearColor::Gray)
					.OnClicked(OnClicked)
					.ContentPadding(FMargin(0.f, 4.f))
					.ToolTipText(TooltipText)
					[
						SNew(SBox)
						.HAlign(HAlign_Center)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								IconWidget
							]
							+ SHorizontalBox::Slot()
							[
								SNew(STextBlock)
								.Text(ButtonText)
								.Justification(ETextJustify::Center)
							]
						]
					]
				]
			];
	}
}

#undef LOCTEXT_NAMESPACE

# Automation tests

## Tests

The automation tests cover the pure layout and the editor layer built on top of it. They live next to the code, in a separate folder:

| File | Contents |
|---|---|
| `Source/GridPlacementTool/Private/Tests/GridPlacementToolLayoutTest.cpp` | Grid layout (`GridPlacement::Layout::Solve`). |
| `Source/GridPlacementTool/Private/Tests/GridPlacementToolMenuHelpersTest.cpp` | Editor layer: settings, button rules, placement and spawning. |
| `Source/GridPlacementTool/Private/Tests/GridPlacementToolTestHelpers.h` | Shared test fixtures; the tests themselves are not here. |

### What is covered

| Test | Verifies |
|---|---|
| `GridPlacementTool.Layout.Solve` | Mapping an index to (row, column), centering with an offset pivot and non-zero scale, negative Spacing, anchor rotation, and discarding invalid actors. |
| `GridPlacementTool.Layout.SizeBasedSpacing` | Rows and columns growing to fit the largest element, in contrast to the same set with uniform spacing. |
| `GridPlacementTool.Layout.FirstRow` | Greedy row packing by the width of the first row, and falling back to a regular grid when Size-based spacing is disabled. |
| `GridPlacementTool.Layout.Sorting` | Sort stability, ascending and descending order, and the "order changed" flag. |
| `GridPlacementTool.Layout.EdgeCases` | Empty input, input consisting only of invalid actors, a single element on an offset anchor, `Columns` greater than the number of elements, and `Columns = 0`. |
| `GridPlacementTool.Menu.BuildPlacementOptions` | That all settings reach the layout, that the first selected actor becomes the anchor regardless of size, and that an invalid first actor falls back to the manual transform. |
| `GridPlacementTool.Menu.ExecutionRules` | The rules that make the Place button active and that choose between the "actors" and "assets" modes. |
| `GridPlacementTool.Menu.PlaceActorsOnGrid` | Placing real editor actors and syncing the selection when sorting changed the order. |
| `GridPlacementTool.Menu.SpawnAssetsOnGrid` | Spawning assets into the level, their landing on the grid, and their selection after placement. |
| `GridPlacementTool.Menu.UndoRedo` | Support for undoing and redoing placement operations through editor transactions (Undo/Redo). |

### The RunTests.ps1 script

The repository root contains a `RunTests.ps1` script that clears the report directory, starts the editor, prints a summary, and prints details for failed tests.

```powershell
.\RunTests.ps1                                   # all plugin tests
.\RunTests.ps1 GridPlacementTool.Menu            # editor layer only
.\RunTests.ps1 GridPlacementTool.Layout -Build   # build the editor target and run the layout tests
```

It searches for the project upwards from its own directory, and for the engine via the project's `EngineAssociation`, falling back to the `UE_ROOT` environment variable. The engine and project directories can be set explicitly with `-UERoot` and `-Project`, and `-ReportDir` overrides the report location.

Return codes: `0` — all tests passed, `1` — some tests failed, `2` — the run did not happen (the build failed, the editor did not start, the report was not created). The full editor log goes to `<report>\editor.log`, the console output to `<report>\console.log`; when startup fails the script prints the errors found in the log, its tail, and a hint if the plugin module simply is not built.

### Running from the editor

`Tools → Session Frontend`, the **Automation** tab: type `GridPlacementTool` in the search box, tick the desired tests, and click **Start Tests**. Results appear in a list below; double-clicking a test shows its events. This same window is the easiest way to see the current list of test names.

### Specifics of these tests

- **All tests are tagged `EditorContext`** — they need a live `GEditor`. They are not available in a packaged game or in a commandlet build.
- **`Menu.PlaceActorsOnGrid` and `Menu.SpawnAssetsOnGrid` work in the editor world.** They spawn real actors into the current level, verify them, and clean up after themselves; assets are spawned through regular actor factories. Both tests call `GEditor->SelectNone` / `SelectActor`, so running them in an open editor resets the scene selection. Nothing is damaged: the actors are transient and get deleted, but it is noticeable in an interactive run.
- **Tool settings are not corrupted.** The `FScopedSettingsOverride` fixture snapshots the fields of `UGridPlacementToolSettings` and restores them in its destructor, and `SaveConfig` is never called — the ini file is not rewritten. Even a failing test will not damage the saved settings.
- **The remaining tests do not touch the editor** — they create a throwaway game world through `FScopedTestWorld`.

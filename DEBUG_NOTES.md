# Debug Notes

## Status as of 2026-05-05

- User goal: plugin should install into Stellarium without any manual DLL/runtime fixes.
- Supported target line is currently `Stellarium 26.1 Qt6`.
- Release build was pinned to `Stellarium v26.1` and `Qt 6.9.3`.

## What was fixed already

- Race condition in aircraft fetch handling:
  - stale replies are ignored
  - previous in-flight reply is aborted before a newer fetch
- Plugin version metadata was unified and bumped up through:
  - `0.0.9`
  - `0.0.10`
  - `0.0.11`
- Workflow now uses concurrency cancellation to avoid overlapping stale runs.

## Loader failures observed

### Old failure

With old Stellarium install:

- Host was `Stellarium 0.21.0`
- Qt runtime was `5.12.6`
- Plugin built for `Stellarium 26.1 Qt6`

Result:

- `Invalid metadata version`

This was expected binary incompatibility.

### Current failure on Stellarium 26.1

After user updated Stellarium to `26.1.0`, the error changed to:

- `Cannot load library ... libStellAirium.dll: Nie można odnaleźć określonego modułu.`

This means plugin metadata is now accepted, but one dependent DLL is missing.

## Root cause found

`dumpbin /dependents` on `%APPDATA%\\Stellarium\\modules\\StellAirium\\libStellAirium.dll` showed:

- direct dependency on `Qt6WebEngineWidgets.dll`

But stock user install at `C:\\Program Files\\Stellarium` does not contain:

- `Qt6WebEngineWidgets.dll`

So plugin was not self-contained relative to the published Stellarium runtime.

## Why this likely happened

In `src/CMakeLists.txt`, Windows plugin build linked against `stelMain`.

For official MSVC Windows builds in Stellarium `v26.1`:

- `GENERATE_STELMAINLIB` is `0`
- `stelMain` is static
- `stellarium.exe` has `ENABLE_EXPORTS TRUE`

Linking the plugin against `stelMain` likely pulled in an oversized dependency set from the host build, including `Qt6WebEngineWidgets.dll`.

## Fix applied for v0.0.11

Windows link logic was changed in `src/CMakeLists.txt`:

- prefer `stellarium` target on Windows when available
- fall back to `stelMain` only if `stellarium` is not present

Intent:

- resolve host symbols from the real exporting target
- avoid dragging unwanted Qt runtime dependencies into `libStellAirium.dll`

## Git state

- Commit: `125fadd`
- Tag: `v0.0.11`

## Workflow state at handoff

Workflow:

- `Build StellAirium plugin`

Run started for:

- tag `v0.0.11`

At handoff time:

- macOS and Windows jobs had started successfully
- Windows build still needed verification after artifact creation

## First checks for next session

1. Check outcome of GitHub Actions run for `v0.0.11`.
2. Download the Windows artifact/release DLL.
3. Run `dumpbin /dependents` on new `libStellAirium.dll`.
4. Confirm `Qt6WebEngineWidgets.dll` is no longer imported.
5. Replace local `%APPDATA%\\Stellarium\\modules\\StellAirium\\libStellAirium.dll`.
6. Launch Stellarium and inspect `%APPDATA%\\Stellarium\\log.txt`.
7. Confirm plugin loads with no manual DLL copying.

---
name: run-eppo-editor
description: Build, run, and launch the Eppo game engine editor (EppoEditor, a C++/Vulkan/ImGui desktop app). Use when asked to build the project, run/launch the editor, or check that the app still starts without crashing.
---

# Run EppoEditor

`EppoEditor` is a Windows C++20 desktop app (Vulkan via NVRHI, ImGui, GLFW).
There is no driver and nothing to automate — it's two commands to build, then
launch the exe. If it starts without crashing, it works; if it crashes, the log
says why.

Paths below are relative to the repo root.

## Prerequisites

These environment variables must be set (configure fails without them):

- `VCPKG_ROOT` — vcpkg checkout
- `VULKAN_SDK` — Vulkan SDK install
- `DOTNET_ROOT` — .NET SDK (builds the managed scripting DLL)

## Build

Two commands (clang-cl via CMake presets):

```powershell
cmake --preset=windows-debug
cmake --build --preset=windows-debug
```

First configure runs `vcpkg install` and can take a while; incremental builds are
fast. Swap `windows-debug` for `windows-release` or `windows-dist` for other
configs. On Linux use the `linux-*` presets.

## Run

Launch the exe **with its own directory as the working directory** — the build
copies `Resources/` and the managed scripting DLL next to the exe, and the app
resolves them relative to the current directory:

```powershell
Start-Process -FilePath .\build\debug\EppoEditor\Debug\EppoEditor.exe `
              -WorkingDirectory .\build\debug\EppoEditor\Debug
```

On startup the editor always opens a native **Open** dialog and blocks on it —
this is a manual step, there is no command-line flag or remembered last project.
You must click through it:

- Pick `EppoEditor/Projects/Test/Test.epproj` to load the sample scene, or
- Cancel / Escape to start with an empty scene.

That's the whole check: if the window comes up and stays up, it runs.

## Tests

The `EppoEngineTesting` target (UnitTest++ runner linking `EppoEngine`)
builds when `BUILD_TESTING` is ON (the default). Run the suite with:

```
cmake --build --preset=windows-debug --target EppoEngineTesting
ctest --test-dir build/debug -C Debug
```

Or run the exe directly: `build/debug/EppoEngineTesting/Debug/EppoEngineTesting.exe`.

## If it crashes

Read the log written next to the exe:

```
build/debug/EppoEditor/Debug/latest.log     # current run
build/debug/EppoEditor/Debug/previous.log   # prior run
```

# TurtleSim

Pure-C++20 simulation core for Gather and Hunt. Zero Unreal Engine dependencies.

See `Docs/LLM/GatherAndHuntDesign.md` for the authoritative spec.

## Build

The build tree goes in the project's `Intermediate/`, **not** under this directory:
UnrealBuildTool compiles every `.cpp` beneath `Source/TurtleSim` into the UE
module, so a `build/` folder here breaks the editor build with a duplicate
`main`. `CMakeLists.txt` refuses to configure in that case.

```
cd Source/TurtleSim
cmake -S . -B ../../Intermediate/TurtleSimCMake
cmake --build ../../Intermediate/TurtleSimCMake
ctest --test-dir ../../Intermediate/TurtleSimCMake --output-on-failure
```

On Windows, run these from a shell with `vcvars64.bat` already sourced.

## Layout

- `Public/` — headers consumed by the UE `TurtleGame` module and tests
- `Private/` — implementation
- `Tests/` — in-tree test runner; one `*Tests.cpp` per subsystem

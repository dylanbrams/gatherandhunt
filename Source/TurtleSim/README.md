# TurtleSim

Pure-C++20 simulation core for Gather and Hunt. Zero Unreal Engine dependencies.

See `Docs/LLM/GatherAndHuntDesign.md` for the authoritative spec.

## Build

```
cd Source/TurtleSim
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Layout

- `Public/` — headers consumed by the UE `TurtleGame` module and tests
- `Private/` — implementation
- `Tests/` — in-tree test runner; one `*Tests.cpp` per subsystem

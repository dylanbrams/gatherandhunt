// Empty UE module for the TurtleSim static lib. The simulation core has no
// UObject types and no engine integration, so we register it as a default
// IModuleInterface implementation. This file is compiled only by UBT; the
// CMake build excludes it.

#if defined(__has_include) && __has_include("Modules/ModuleInterface.h")
#include "Modules/ModuleManager.h"
IMPLEMENT_MODULE(FDefaultModuleImpl, TurtleSim)
#endif

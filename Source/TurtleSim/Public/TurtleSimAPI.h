#pragma once

// TURTLESIM_API expansion. UBT auto-generates this macro for the TurtleSim module
// (dllexport when compiling TurtleSim, dllimport when consuming). The fallback
// below makes the macro a no-op for the standalone CMake build, which links
// statically and doesn't need export decorations.

#if !defined(DLLEXPORT)
	#if defined(_MSC_VER)
		#define DLLEXPORT __declspec(dllexport)
	#else
		#define DLLEXPORT
	#endif
#endif

#if !defined(DLLIMPORT)
	#if defined(_MSC_VER)
		#define DLLIMPORT __declspec(dllimport)
	#else
		#define DLLIMPORT
	#endif
#endif

#ifndef TURTLESIM_API
#define TURTLESIM_API
#endif

#pragma once
//////////////////////////////////////////////////////////////////////
// An experiment custom scripting language for The Force Engine.
// This is a long-term project and should not interfere with
// reverse-engineering work or the Feb. test release.
//
// Reasons for using this over AngelScript:
//   * Keep C-like syntax but make it more non-programmer friendly
//   * Implicit line endings.
//   * Implicit type based on usage.
//   * Fewer types in general.
//   * More direct integration - internal editing and debugging.
//   * Removes AngelCode, which is a LOT of code and a large dependency.
//   * More direct control.
//   * Domain specific - only include desired features.
// Reaons this might be a bad idea:
//   * Performance - this needs to be competitive with Angel Script.
//   * More potential for bugs.
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

typedef u32 TFS_ModuleHandle;
typedef u32 TFS_InstanceHandle;
#define TFS_INVALID_HANDLE 0xffffffff

namespace TFE_ForceScript
{
	bool init();
	void destroy();

	// Load and compile a module from the source file.
	// Cached to avoid recompiling.
	TFS_ModuleHandle loadModule(const char* sourceFile, bool forceRecompile = false);

	// Load and compile a module from memory.
	TFS_ModuleHandle loadModuleFromMemory(const char* buffer);

	// Create an instance of a module, this holds its own local state.
	TFS_InstanceHandle createModuleInstance(TFS_ModuleHandle module);

	// Later specific functions should be executed, but this is here for early testing.
	void execute(TFS_InstanceHandle instance);
}
#include "forceScript.h"
#include "tfs_compiler.h"
#include <TFE_System/system.h>
#include <TFE_FileSystem/filestream.h>
#include <TFE_FileSystem/paths.h>
#include <TFE_FrontEndUI/console.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <algorithm>

namespace TFE_ForceScript
{
	std::vector<char> s_buffer;
	
	bool init()
	{
		// Test - obviously hardcoded paths are bad...
		loadModule("D:/dev/TheForceEngine/TheForceEngine/TFE_ScriptSystem/ForceScript/devScript.tfs");

		return true;
	}

	void destroy()
	{
	}
		
	// Load and compile a module from the source file.
	// Cached to avoid recompiling.
	TFS_ModuleHandle loadModule(const char* sourceFile, bool forceRecompile/*=false*/)
	{
		FileStream file;
		if (!file.open(sourceFile, FileStream::MODE_READ))
		{
			return TFS_INVALID_HANDLE;
		}

		// Read the file.
		size_t len = file.getSize();
		s_buffer.resize(len + 1);
		char* buffer = s_buffer.data();
		file.readBuffer(buffer, len);
		buffer[len] = 0;
		file.close();

		TFS_Compiler::compile(buffer, len);

		return TFS_INVALID_HANDLE;
	}

	// Load and compile a module from memory.
	TFS_ModuleHandle loadModuleFromMemory(const char* buffer)
	{
		return TFS_INVALID_HANDLE;
	}

	// Create an instance of a module, this holds its own local state.
	TFS_InstanceHandle createModuleInstance(TFS_ModuleHandle module)
	{
		return TFS_INVALID_HANDLE;
	}

	// Later specific functions should be executed, but this is here for early testing.
	void execute(TFS_InstanceHandle instance)
	{
	}
}
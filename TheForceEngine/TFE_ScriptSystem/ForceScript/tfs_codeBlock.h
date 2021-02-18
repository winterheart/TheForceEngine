#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>

namespace TFE_ForceScript
{
	struct Value;

	struct ValueArray
	{
		Value* values;
		s32 size;
		s32 capacity;
	};

	struct CodeBlock
	{
		u8* code;
		s32 size;
		s32 capacity;

		ValueArray constants;
	};
}
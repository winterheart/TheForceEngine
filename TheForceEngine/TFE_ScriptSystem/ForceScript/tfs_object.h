#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "tfs_codeBlock.h"
#include "tfs_hashTable.h"

namespace TFE_ForceScript
{
	enum ObjType
	{
		OBJ_STRING = 0,
		OBJ_FUNC,
		OBJ_NATIVE,
		OBJ_STRUCT,
		OBJ_STRUCT_INST,
		OBJ_COUNT
	};

	struct Object
	{
		ObjType type;
		Object* next;
	};

	struct ObjString
	{
		Object obj;
		s32 length;
		char* buffer;
		u32 hash;
	};

	struct ObjFunc
	{
		Object obj;
		s32 arity;	// number of arguments.
		// For now - do we want to keep it seperate in the future?
		CodeBlock  code;
		ObjString* name;
	};

	typedef Value(*NativeFn)(s32 argCount, Value* args);

	struct ObjNative
	{
		Object obj;
		NativeFn func;
	};

	// Eventually struct definitions should not be objects and field offsets should
	// be handled at compile time.
	struct ObjStruct
	{
		Object obj;
		ObjString* name;	// name of the struct.
		Table fields;		// field names, types, and offsets.
	};

	struct ObjStructInst
	{
		Object obj;
		ObjStruct* def;		// struct definition.
		Value* fields;		// field values.
	};

	inline ObjType objType(Value value)
	{
		return value.ovalue->type;
	}

	inline bool isString(Value value)
	{
		return value.type == VALUE_OBJ && value.ovalue->type == OBJ_STRING;
	}

	inline bool isFunc(Value value)
	{
		return value.type == VALUE_OBJ && value.ovalue->type == OBJ_FUNC;
	}
	   
	inline ObjString* asString(Value value)
	{
		return (ObjString*)value.ovalue;
	}

	inline char* asCString(Value value)
	{
		return ((ObjString*)value.ovalue)->buffer;
	}

	inline ObjFunc* asFunc(Value value)
	{
		return (ObjFunc*)value.ovalue;
	}

	inline bool isNative(Value value)
	{
		return value.type == VALUE_OBJ && value.ovalue->type == OBJ_NATIVE;
	}

	inline ObjNative* asNative(Value value)
	{
		return (ObjNative*)value.ovalue;
	}

	inline bool isStruct(Value value)
	{
		return value.type == VALUE_OBJ && value.ovalue->type == OBJ_STRUCT;
	}

	inline ObjStruct* asStruct(Value value)
	{
		return (ObjStruct*)value.ovalue;
	}

	inline bool isStructInst(Value value)
	{
		return value.type == VALUE_OBJ && value.ovalue->type == OBJ_STRUCT_INST;
	}

	inline ObjStructInst* asStructInst(Value value)
	{
		return (ObjStructInst*)value.ovalue;
	}
}
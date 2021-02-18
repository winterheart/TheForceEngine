#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "tfs_codeBlock.h"

namespace TFE_ForceScript
{
	struct Object;

	enum ValueType
	{
		VALUE_NULL = 0,
		VALUE_FLOAT,
		VALUE_INT,
		VALUE_BOOL,
		VALUE_OBJ,
		VALUE_COUNT
	};

	struct Value
	{
		u8 type;
		union
		{
			f32 fvalue;
			s32 ivalue;
			bool bvalue;
			Object* ovalue;
			size_t valueBits;
		};
	};

	// Values
	inline Value boolValue(bool value) { Value tmp; tmp.type = VALUE_BOOL; tmp.bvalue = value; return tmp; }
	inline Value floatValue(f32 value) { Value tmp; tmp.type = VALUE_FLOAT; tmp.fvalue = value; return tmp; }
	inline Value intValue(s32 value) { Value tmp; tmp.type = VALUE_INT; tmp.ivalue = value; return tmp; }
	inline Value objValue(Object* value) { Value tmp; tmp.type = VALUE_OBJ; tmp.ovalue = value; return tmp; }
	inline Value nullValue() { Value tmp; tmp.type = VALUE_NULL; tmp.ivalue = 0; return tmp; }

	inline bool asBool(Value value)   { return value.bvalue; }
	inline f32 asFloat(Value value)   { return value.fvalue; }
	inline s32 asInt(Value value)     { return value.ivalue; }
	inline Object* asObj(Value value) { return value.ovalue; }

	inline f32 castToFloat(Value value)
	{
		switch (value.type)
		{
			case VALUE_FLOAT:
				return value.fvalue;
				break;
			case VALUE_INT:
				return (f32)value.ivalue;
				break;
			case VALUE_BOOL:
				return value.bvalue ? 1.0f : 0.0f;
				break;
		}
		return 0.0f;
	}

	inline s32 castToInt(Value value)
	{
		switch (value.type)
		{
		case VALUE_FLOAT:
			return (s32)value.fvalue;
			break;
		case VALUE_INT:
			return value.ivalue;
			break;
		case VALUE_BOOL:
			return value.bvalue ? 1 : 0;
			break;
		}
		return 0;
	}

	inline bool castToBool(Value value)
	{
		switch (value.type)
		{
		case VALUE_FLOAT:
			return value.fvalue != 0.0f;
			break;
		case VALUE_INT:
			return value.ivalue != 0;
			break;
		case VALUE_BOOL:
			return value.bvalue;
			break;
		case VALUE_OBJ:
			return value.ovalue != nullptr;
			break;
		}
		return 0;
	}

	inline bool isBool(Value value)  { return value.type == VALUE_BOOL; }
	inline bool isFloat(Value value) { return value.type == VALUE_FLOAT; }
	inline bool isInt(Value value)   { return value.type == VALUE_INT; }
	inline bool isObj(Value value)   { return value.type == VALUE_OBJ; }
	inline bool isNull(Value value)  { return value.type == VALUE_NULL; }
}
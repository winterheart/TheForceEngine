#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "tfs_value.h"
#include "tfs_object.h"
#include "tfs_codeBlock.h"

namespace TFE_ForceScript
{
	struct CodeBlock;

	enum OpCode
	{
		OP_RET = 0,
		OP_POP,
		OP_DEFINE_GLOBAL,
		OP_GET_GLOBAL,
		OP_SET_GLOBAL,
		OP_GET_LOCAL,
		OP_SET_LOCAL,
		OP_STRUCT,
		OP_FIELD,
		OP_SET_FIELD,
		OP_GET_FIELD,
		OP_JUMP,
		OP_LOOP,
		OP_JUMP_IF_FALSE,
		OP_JUMP_IF_TRUE,
		OP_CALL,
		OP_NULL,
		OP_CONSTANT,
		OP_TRUE,
		OP_FALSE,
		// Unary
		OP_NEGATE,
		OP_NOT,
		OP_INC,
		OP_DEC,
		// Binary
		OP_EQ,
		OP_GREATER,
		OP_LESS,
		OP_NOT_EQ,
		OP_GREATER_EQ,
		OP_LESS_EQ,
		OP_ADD,
		OP_SUB,
		OP_MUL,
		OP_DIV,
		OP_PRINT,
		OP_COUNT
	};

	enum ExecuteResult
	{
		EXE_FAIL = 0,
		EXE_SUCCESS,
	};

	struct CallFrame
	{
		ObjFunc* func;
		u8* ip;
		Value* slots;
	};
				
	namespace TFS_VM
	{
		void init();
		void destroy();
		ExecuteResult execute(ObjFunc* func);

		CodeBlock* initCodeBlock();
		void freeCodeBlock(CodeBlock* block);
		void writeCode(CodeBlock* block, s32 value);

		s32 addConstant(CodeBlock* block, Value value);
		void defineNativeFunc(const char* name, NativeFn func);

		// Debug
		void disassembleCodeBlock(CodeBlock* block, const char* name);
		
		ObjString* allocateString(char* buffer, s32 length, u32 hash);
		ObjString* copyString(const char* buffer, s32 length);
		ObjFunc* allocateFunction();
		ObjNative* allocateNative(NativeFn func);
		ObjString* getInterned(char* buffer, s32 length, u32 hash);
	}
}
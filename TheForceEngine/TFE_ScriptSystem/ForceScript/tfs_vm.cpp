#include "tfs_vm.h"
#include "tfs_compiler.h"
#include "tfs_lexer.h"
#include "tfs_hashTable.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/console.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>

using namespace TFE_ForceScript::TFS_HashTable;

namespace TFE_ForceScript
{
	// #define DEBUG_TRACE_EXECUTION 1
	#define MAX_FRAMES 64
	#define MAX_STACK (256 * MAX_FRAMES)

	struct VM
	{
		//CodeBlock* block;
		//u8* ip;

		CallFrame frames[MAX_STACK];
		s32 frameCount;

		Value stack[MAX_STACK];
		Value* stackTop;
		// String table, where each string is unique.
		Table strings;
		Table globals;

		Object* objects;
	};
		
	namespace TFS_VM
	{
		static const size_t c_initCodeblockSize = 64;
		static VM s_vm = { 0 };
		static CallFrame* s_frame = nullptr;
		static f64 s_startTime = 0.0;

		void initValueArray(ValueArray* array);
		void freeValueArray(ValueArray* array);
		void writeValueArray(ValueArray* array, Value value);

		s32 disassembleInstruction(CodeBlock* block, s32 offset);
		void freeObject(Object* obj);

		void initCodeBlock_NoAlloc(CodeBlock* block);

		// TODO: Build a function mapping to avoid having to do this manually.
		Value clockNative(s32 argCount, Value* args)
		{
			f64 curTime = TFE_System::convertFromTicksToSeconds(TFE_System::getCurrentTimeInTicks());
			f32 time = f32(curTime - s_startTime);
			return floatValue(time);
		}

		Value sqrtNative(s32 argCount, Value* args)
		{
			if (argCount)
			{
				f32 x = asFloat(args[0]);
				return floatValue(sqrtf(x));
			}
			return floatValue(0.0f);
		}

		//////////////////////////////////////////////////////////////////////
		// Generated code for FFI concept:
		#define SCRIPT_EXPORT(name, ...)

		// in include file:
		SCRIPT_EXPORT(sqrt) f32 sqrt(f32 x) { return 0.0f; }
		// in script: sqrt(x)

		// Library:
		typedef Value(*NativeFuncPtr)(Value*);
		struct NativeFunc
		{
			NativeFuncPtr func;
			s32 argCount;
		};

		void registerNativeFunc(const char* name, NativeFuncPtr func, s32 argCount) {}

		// This should be inlined to avoid adding the overhead of a third function call.
		Value callTest(NativeFunc* nativeFunc, s32 argCount, Value* args)
		{
			if (argCount != nativeFunc->argCount)
			{
				// error
			}
			return nativeFunc->func(args);
		}

		// Generated code:
		Value __sqrt(Value* args)
		{
			return floatValue(sqrt(castToFloat(args[0])));
		}

		void nativeFuncSetup()
		{
			registerNativeFunc("sqrt", __sqrt, 1);
		}
		//////////////////////////////////////////////////////////////////////

		void init()
		{
			s_vm.objects = nullptr;
			s_vm.frameCount = 0;
			initTable(&s_vm.strings);
			initTable(&s_vm.globals);
			s_vm.stackTop = s_vm.stack;
			s_startTime = TFE_System::convertFromTicksToSeconds(TFE_System::getCurrentTimeInTicks());

			// Native functions.
			defineNativeFunc("clock", clockNative);
			defineNativeFunc("sqrt", sqrtNative);
		}

		void destroy()
		{
			freeTable(&s_vm.globals);
			freeTable(&s_vm.strings);

			Object* obj = s_vm.objects;
			while (obj)
			{
				Object* next = obj->next;
				freeObject(obj);
				obj = next;
			}
			s_vm.objects = nullptr;
		}

		u8 readByte()
		{
			return *s_frame->ip++;
		}

		u16 readShort()
		{
			s_frame->ip += 2;
			return (s_frame->ip[-2] << 8) | (s_frame->ip[-1]);
		}
						
		void push(Value value)
		{
			*s_vm.stackTop = value;
			s_vm.stackTop++;
		}

		Value pop()
		{
			s_vm.stackTop--;
			return *s_vm.stackTop;
		}

		Value peek(s32 offset)
		{
			return s_vm.stackTop[-1 - offset];
		}

		void freeObject(Object* obj)
		{
			switch (obj->type)
			{
				case OBJ_STRING:
				{
					ObjString* string = (ObjString*)obj;
					free(string->buffer);
					free(string);
				} break;
				case OBJ_FUNC:
				{
					ObjFunc* func = (ObjFunc*)obj;
					freeCodeBlock(&func->code);
					free(func);
				} break;
				case OBJ_NATIVE:
				{
					ObjNative* native = (ObjNative*)obj;
					free(native);
				} break;
				case OBJ_STRUCT:
				{
					ObjStruct* def = (ObjStruct*)obj;
					freeTable(&def->fields);
					free(def);
				} break;
				case OBJ_STRUCT_INST:
				{
					ObjStructInst* inst = (ObjStructInst*)obj;
					free(inst->fields);
				} break;
			}
		}

		void printStacktrace()
		{
			TFE_Console::print("/c8080ff === Stack Trace ===");
			for (s32 i = s_vm.frameCount - 1; i >= 0; i--)
			{
				CallFrame* frame = &s_vm.frames[i];
				ObjFunc* func = frame->func;
				s32 instr = s32(frame->ip - func->code.code - 1);
				// TODO: Proper line numbers.
				TFE_Console::print("/c8040ff [line %d] in %s()", 0, func->name ? func->name->buffer : "SCRIPT");
			}
		}
				
		void runtimeError(const char* format, ...)
		{
			static char s_buffer[1024];

			va_list arg;
			va_start(arg, format);
			vsprintf(s_buffer, format, arg);
			va_end(arg);

			TFE_Console::print("/cff4040 Error on line %d: %s", 0, s_buffer);
			printStacktrace();
		}

		void defineNativeFunc(const char* name, NativeFn func)
		{
			// TODO: Interfacing with the stack shouldn't be necessary - simply adding it to the table should be fine.
			// Note: it might make more sense to have a list of functions that can be called instead of using globals.
			push(objValue((Object*)copyString(name, (s32)strlen(name))));
			push(objValue((Object*)allocateNative(func)));
			tableSet(&s_vm.globals, asString(s_vm.stack[0]), s_vm.stack[1]);
			pop();
			pop();
		}

		bool isFalse(Value value)
		{
			switch (value.type)
			{
				case VALUE_BOOL:  return !value.bvalue; break;
				case VALUE_FLOAT: return value.fvalue ? false : true; break;
				case VALUE_INT:   return value.ivalue ? false : true; break;
				case VALUE_OBJ:   return value.ovalue ? false : true; break;
				case VALUE_NULL:  return true; break;
			}
			return true;
		}

		bool valuesEq(Value a, Value b)
		{
			if (a.type != b.type) { return false; }

			switch (a.type)
			{
				case VALUE_BOOL:  return asBool(a) == asBool(b); break;
				case VALUE_FLOAT: return asFloat(a) == asFloat(b); break;
				case VALUE_INT:   return asInt(a) == asInt(b); break;
				case VALUE_OBJ:   return asObj(a) == asObj(b); break;
			}
			return false;
		}

		bool less(Value a, Value b)
		{
			if (a.type != b.type)
			{
				runtimeError("Operands must be the same type.");
				return false;
			}

			switch (a.type)
			{
				case VALUE_FLOAT: push(boolValue(asFloat(a) < asFloat(b))); return true; break;
				case VALUE_INT:   push(boolValue(asInt(a) < asInt(b))); return true; break;
				default:
				{
					runtimeError("Operands must be numbers.");
				}
			}
			return false;
		}

		bool lessEq(Value a, Value b)
		{
			if (a.type != b.type)
			{
				runtimeError("Operands must be the same type.");
				return false;
			}

			switch (a.type)
			{
			case VALUE_FLOAT: push(boolValue(asFloat(a) <= asFloat(b))); return true; break;
			case VALUE_INT:   push(boolValue(asInt(a) <= asInt(b))); return true; break;
			default:
			{
				runtimeError("Operands must be numbers.");
			}
			}
			return false;
		}

		bool greater(Value a, Value b)
		{
			if (a.type != b.type)
			{
				runtimeError("Operands must be the same type.");
				return false;
			}

			switch (a.type)
			{
			case VALUE_FLOAT: push(boolValue(asFloat(a) > asFloat(b))); return true; break;
			case VALUE_INT:   push(boolValue(asInt(a) > asInt(b))); return true; break;
			default:
			{
				runtimeError("Operands must be numbers.");
			}
			}
			return false;
		}
		bool greaterEq(Value a, Value b)
		{
			if (a.type != b.type)
			{
				runtimeError("Operands must be the same type.");
				return false;
			}

			switch (a.type)
			{
				case VALUE_FLOAT: push(boolValue(asFloat(a) >= asFloat(b))); return true; break;
				case VALUE_INT:   push(boolValue(asInt(a) >= asInt(b))); return true; break;
				default:
				{
					runtimeError("Operands must be numbers.");
				}
			}
			return false;
		}

		void printObject(char* buffer, Value value)
		{
			switch (value.ovalue->type)
			{
				case OBJ_STRING:
					sprintf(buffer, "\"%s\"", asCString(value));
					break;
				case OBJ_FUNC:
					sprintf(buffer, "<fn %s>", asFunc(value)->name->buffer);
					break;
				case OBJ_NATIVE:
					sprintf(buffer, "<native fn>");
					break;
				default:
					sprintf(buffer, "%p", value.ovalue);
			}
		}

		void printValue(char* buffer, Value value)
		{
			switch (value.type)
			{
				case VALUE_FLOAT:
					sprintf(buffer, "%f", value.fvalue);
					break;
				case VALUE_INT:
					sprintf(buffer, "%d", value.ivalue);
					break;
				case VALUE_BOOL:
					sprintf(buffer, "%s", value.bvalue ? "true" : "false");
					break;
				case VALUE_OBJ:
					printObject(buffer, value);
					break;
			}
		}

		ObjString* allocateString(char* buffer, s32 length, u32 hash)
		{
			ObjString* string = (ObjString*)malloc(sizeof(ObjString));
			string->buffer = buffer;
			string->length = length;
			string->hash = hash;
			string->obj.type = OBJ_STRING;
			string->obj.next = s_vm.objects;

			// Add the string to the hash table.
			tableSet(&s_vm.strings, string, nullValue());

			s_vm.objects = (Object*)string;

			return string;
		}

		ObjString* copyString(const char* buffer, s32 length)
		{
			if (buffer[0] == '"')
			{
				buffer++;
				length--;
			}
			if (buffer[length - 1] == '"')
			{
				length--;
			}

			char* newBuffer = (char*)malloc(length + 1);
			memcpy(newBuffer, buffer, length);
			newBuffer[length] = 0;

			u32 hash = hashString(newBuffer, length);
			ObjString* interned = getInterned(newBuffer, length, hash);
			if (interned)
			{
				free(newBuffer);
				return interned;
			}

			return allocateString((char*)newBuffer, length, hash);
		}

		ObjFunc* allocateFunction()
		{
			ObjFunc* func = (ObjFunc*)malloc(sizeof(ObjFunc));
			func->obj.type = OBJ_FUNC;
			func->obj.next = s_vm.objects;

			func->arity = 0;
			func->name = nullptr;
			initCodeBlock_NoAlloc(&func->code);
			return func;
		}

		ObjNative* allocateNative(NativeFn func)
		{
			ObjNative* native = (ObjNative*)malloc(sizeof(ObjNative));
			native->obj.type = OBJ_NATIVE;
			native->obj.next = s_vm.objects;

			native->func = func;
			return native;
		}

		ObjStruct* allocateStruct(ObjString* name)
		{
			ObjStruct* strct = (ObjStruct*)malloc(sizeof(ObjStruct));
			strct->obj.type = OBJ_STRUCT;
			strct->obj.next = s_vm.objects;

			initTable(&strct->fields);
			strct->name = name;
			return strct;
		}

		ObjStructInst* allocateStructInst(ObjStruct* def)
		{
			ObjStructInst* inst = (ObjStructInst*)malloc(sizeof(ObjStructInst));
			inst->obj.type = OBJ_STRUCT_INST;
			inst->obj.next = s_vm.objects;

			inst->def = def;
			s32 fieldCount = tableGetCount(&def->fields);
			inst->fields = (Value*)malloc(sizeof(Value) * fieldCount);
			memset(inst->fields, 0, sizeof(Value) * fieldCount);

			return inst;
		}

		ObjString* getInterned(char* buffer, s32 length, u32 hash)
		{
			return tableFindString(&s_vm.strings, buffer, length, hash);
		}

		ObjString* takeString(char* buffer, s32 length)
		{
			u32 hash = hashString(buffer, length);
			ObjString* interned = getInterned(buffer, length, hash);
			if (interned)
			{
				free(buffer);
				return interned;
			}

			return allocateString(buffer, length, hash);
		}

		void concatenate()
		{
			ObjString* b = asString(pop());
			ObjString* a = asString(pop());

			s32 length = a->length + b->length;
			char* buffer = (char*)malloc(length + 1);
			memcpy(buffer, a->buffer, a->length);
			memcpy(buffer + a->length, b->buffer, b->length);
			buffer[length] = 0;

			ObjString* result = takeString(buffer, length);
			Value value;
			value.type = VALUE_OBJ;
			value.ovalue = (Object*)result;
			push(value);
		}

		Value readConstant()
		{
			return s_frame->func->code.constants.values[readByte()];
		}

		ObjString* readString()
		{
			return asString(readConstant());
		}
					
		bool call(ObjFunc* func, s32 argCount)
		{
			if (argCount != func->arity)
			{
				runtimeError("Expected %d arguments but got %d.", func->arity, argCount);
				return false;
			}

			if (s_vm.frameCount == MAX_FRAMES)
			{
				runtimeError("Stack overflow.");
				return false;
			}

			CallFrame* frame = &s_vm.frames[s_vm.frameCount++];
			frame->func = func;
			frame->ip = func->code.code;
			frame->slots = s_vm.stackTop - argCount - 1;
			return true;
		}

		bool callValue(Value callee, s32 argCount)
		{
			if (!isObj(callee))
			{
				runtimeError("Can only call functions.");
				return false;
			}

			Object* obj = asObj(callee);
			switch (obj->type)
			{
				case OBJ_FUNC:
				{
					return call(asFunc(callee), argCount);
				} break;
				case OBJ_NATIVE:
				{
					NativeFn native = asNative(callee)->func;
					Value result = native(argCount, s_vm.stackTop - argCount);
					s_vm.stackTop -= argCount + 1;
					push(result);
					return true;
				} break;
				case OBJ_STRUCT:
				{
					// Create a new instance of struct.
					// TODO: This should be a local variable on the stack...
					ObjStruct* def = asStruct(callee);
					s_vm.stackTop[-argCount - 1] = objValue((Object*)allocateStructInst(def));
					return true;
				} break;
				default:
					// non-callable object type.
					break;
			}

			runtimeError("Can only call functions.");
			return false;
		}

		void defineField(ObjString* name)
		{
			Value index = peek(0);
			ObjStruct* def = asStruct(peek(1));
			tableSet(&def->fields, name, index);
			pop();
		}

		ExecuteResult execute(ObjFunc* func)
		{
			push(objValue((Object*)func));
			callValue(objValue((Object*)func), 0);

			s_frame = &s_vm.frames[s_vm.frameCount - 1];
			
			// debug
			char tmp[256];
			disassembleCodeBlock(&s_frame->func->code, "DevScript");

			for (;;)
			{
			#ifdef DEBUG_TRACE_EXECUTION
				disassembleInstruction(s_vm.block, (s32)(s_vm.ip - s_vm.block->code));
			#endif

				u8 instr = readByte();
				switch (instr)
				{
					case OP_RET:
					{
						Value result = pop();

						s_vm.frameCount--;
						if (s_vm.frameCount == 0)
						{
							pop();
							return EXE_SUCCESS;
						}

						s_vm.stackTop = s_frame->slots;
						push(result);

						s_frame = &s_vm.frames[s_vm.frameCount - 1];
					} break;
					case OP_POP:
					{
						pop();
					} break;
					case OP_DEFINE_GLOBAL:
					{
						ObjString* name = readString();
						tableSet(&s_vm.globals, name, peek(0));
						pop();
					} break;
					case OP_GET_GLOBAL:
					{
						ObjString* name = readString();
						Value value;
						if (!tableGet(&s_vm.globals, name, &value))
						{
							runtimeError("Undefined variable '%s'.", name->buffer);
							return EXE_FAIL;
						}
						push(value);
					} break;
					case OP_SET_GLOBAL:
					{
						ObjString* name = readString();
						// tableSet() returns true if it inserts the key.
						if (tableSet(&s_vm.globals, name, peek(0)))
						{
							tableDelete(&s_vm.globals, name);
							runtimeError("Undefined variable '%s'.", name->buffer);
							return EXE_FAIL;
						}
					} break;
					case OP_GET_LOCAL:
					{
						u8 slot = readByte();
						push(s_frame->slots[slot]);
					} break;
					case OP_SET_LOCAL:
					{
						u8 slot = readByte();
						s_frame->slots[slot] = peek(0);
					} break;
					case OP_STRUCT:
					{
						// TODO: This should be handled during compilation instead of at runtime.
						push(objValue((Object*)allocateStruct(readString())));
					} break;
					case OP_FIELD:
					{
						defineField(readString());
					} break;
					case OP_SET_FIELD:
					{
						if (!isStructInst(peek(1)))
						{
							runtimeError("Only struct instances have fields.");
							return EXE_FAIL;
						}

						ObjStructInst* inst = asStructInst(peek(1));
						ObjString* name = readString();
						Value value = pop();

						Value offset;
						if (tableGet(&inst->def->fields, name, &offset))
						{
							pop();
							inst->fields[asInt(offset)] = value;
							push(value);
						}
						else
						{
							runtimeError("Undefined field '%s' in struct '%s'", name->buffer, inst->def->name->buffer);
							return EXE_FAIL;
						}
					} break;
					case OP_GET_FIELD:
					{
						if (!isStructInst(peek(0)))
						{
							runtimeError("Only struct instances have fields.");
							return EXE_FAIL;
						}

						ObjStructInst* inst = asStructInst(peek(0));
						ObjString* name = readString();

						Value offset;
						if (tableGet(&inst->def->fields, name, &offset))
						{
							pop();
							push(inst->fields[asInt(offset)]);
						}
						else
						{
							runtimeError("Undefined field '%s' in struct '%s'", name->buffer, inst->def->name->buffer);
							return EXE_FAIL;
						}
					} break;
					case OP_JUMP:
					{
						const u16 offset = readShort();
						s_frame->ip += offset;
					} break;
					case OP_LOOP:
					{
						const u16 offset = readShort();
						s_frame->ip -= s32(offset);
					} break;
					case OP_JUMP_IF_FALSE:
					{
						const u16 offset = readShort();
						if (isFalse(peek(0)))
						{
							s_frame->ip += offset;
						}
					} break;
					case OP_JUMP_IF_TRUE:
					{
						const u16 offset = readShort();
						if (!isFalse(peek(0)))
						{
							s_frame->ip += offset;
						}
					} break;
					case OP_CALL:
					{
						s32 argCount = readByte();
						if (!callValue(peek(argCount), argCount))
						{
							return EXE_FAIL;
						}
						s_frame = &s_vm.frames[s_vm.frameCount - 1];
					} break;
					case OP_CONSTANT:
					{
						const u8 constantId = readByte();
						const Value value = s_frame->func->code.constants.values[constantId];
						push(value);
					} break;
					case OP_NULL:
					{
						push(nullValue());
					} break;
					case OP_FALSE:
					{
						push(boolValue(false));
					} break;
					case OP_TRUE:
					{
						push(boolValue(true));
					} break;
					// Unary
					case OP_NEGATE:
					{
						if (!isFloat(peek(0)))
						{
							runtimeError("Operand must be a number.");
							return EXE_FAIL;
						}

						Value value = pop();
						push(floatValue(-asFloat(value)));
					} break;
					case OP_NOT:
					{
						push(boolValue(isFalse(pop())));
					} break;
					case OP_INC:
					{
						if (isFloat(peek(0)))
						{
							Value value = pop();
							push(floatValue(asFloat(value) + 1.0f));
						}
						else if (isInt(peek(0)))
						{
							Value value = pop();
							push(intValue(asInt(value) + 1));
						}
						else
						{
							runtimeError("Operand must be a number.");
							return EXE_FAIL;
						}
					} break;
					case OP_DEC:
					{
						if (isFloat(peek(0)))
						{
							Value value = pop();
							push(floatValue(asFloat(value) - 1.0f));
						}
						else if (isInt(peek(0)))
						{
							Value value = pop();
							push(intValue(asInt(value) - 1));
						}
						else
						{
							runtimeError("Operand must be a number.");
							return EXE_FAIL;
						}
					} break;
					// Binary
					case OP_EQ:
					{
						Value b = pop();
						Value a = pop();
						push(boolValue(valuesEq(a, b)));
					} break;
					case OP_NOT_EQ:
					{
						Value b = pop();
						Value a = pop();
						push(boolValue(!valuesEq(a, b)));
					} break;
					case OP_GREATER:
					{
						if (!greater(pop(), pop()))
						{
							return EXE_FAIL;
						}
					} break;
					case OP_LESS:
					{
						if (!less(pop(), pop()))
						{
							return EXE_FAIL;
						}
					} break;
					case OP_GREATER_EQ:
					{
						if (!greaterEq(pop(), pop()))
						{
							return EXE_FAIL;
						}
					} break;
					case OP_LESS_EQ:
					{
						if (!lessEq(pop(), pop()))
						{
							return EXE_FAIL;
						}
					} break;
					case OP_ADD:
					{
						if (isString(peek(0)) && isString(peek(1)))
						{
							concatenate();
						}
						else if (isFloat(peek(0)) && isFloat(peek(1)))
						{
							f32 value1 = asFloat(pop());
							f32 value0 = asFloat(pop());
							Value result = floatValue(value0 + value1);
							push(result);
						}
						else
						{
							runtimeError("Operands must be a numbers or strings.");
							return EXE_FAIL;
						}
					} break;
					case OP_SUB:
					{
						if (!isFloat(peek(0)) || !isFloat(peek(1)))
						{
							runtimeError("Operands must be a numbers.");
							return EXE_FAIL;
						}

						f32 value1 = asFloat(pop());
						f32 value0 = asFloat(pop());
						Value result = floatValue(value0 - value1);
						push(result);
					} break;
					case OP_MUL:
					{
						if (!isFloat(peek(0)) || !isFloat(peek(1)))
						{
							runtimeError("Operands must be a numbers.");
							return EXE_FAIL;
						}

						f32 value1 = asFloat(pop());
						f32 value0 = asFloat(pop());
						Value result = floatValue(value0 * value1);
						push(result);
					} break;
					case OP_DIV:
					{
						if (!isFloat(peek(0)) || !isFloat(peek(1)))
						{
							runtimeError("Operands must be a numbers.");
							return EXE_FAIL;
						}

						f32 value1 = asFloat(pop());
						f32 value0 = asFloat(pop());
						Value result = floatValue(value0 / value1);
						push(result);
					} break;
					case OP_PRINT:
					{
						printValue(tmp, pop());
						TFE_Console::print("/c40ff40 %s", tmp);
					} break;
				}
			}

			return EXE_FAIL;
		}

		void initCodeBlock_NoAlloc(CodeBlock* block)
		{
			block->code = (u8*)malloc(c_initCodeblockSize);
			memset(block->code, 0, c_initCodeblockSize);
			block->capacity = c_initCodeblockSize;
			block->size = 0;

			initValueArray(&block->constants);
		}

		CodeBlock* initCodeBlock()
		{
			CodeBlock* block = (CodeBlock*)malloc(sizeof(CodeBlock));
			initCodeBlock_NoAlloc(block);
			return block;
		}

		void freeCodeBlock(CodeBlock* block)
		{
			if (!block) { return; }

			free(block->code);
			freeValueArray(&block->constants);
			free(block);
		}

		void writeCode(CodeBlock* block, s32 value)
		{
			if (block->size + 1 > block->capacity)
			{
				block->capacity *= 2;
				block->code = (u8*)realloc(block->code, block->capacity);
			}
			block->code[block->size++] = value;
		}

		s32 addConstant(CodeBlock* block, Value value)
		{
			writeValueArray(&block->constants, value);
			return block->constants.size - 1;
		}

		////////////////////////////////////////////////////////////////////////////////
		////////////////////////////// Variables ///////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////
		void initValueArray(ValueArray* array)
		{
			array->values = nullptr;
			array->capacity = 0;
			array->size = 0;
		}

		void freeValueArray(ValueArray* array)
		{
			free(array->values);
			initValueArray(array);
		}

		void writeValueArray(ValueArray* array, Value value)
		{
			if (array->size + 1 > array->capacity)
			{
				array->capacity = (array->capacity == 0) ? 16 : array->capacity*2;
				array->values = (Value*)realloc(array->values, array->capacity * sizeof(Value));
			}
			array->values[array->size++] = value;
		}

		////////////////////////////////////////////////////////////////////////////////
		////////////////////////////////// DEBUG ///////////////////////////////////////
		////////////////////////////////////////////////////////////////////////////////
		static char s_output[1024];

		void disassembleCodeBlock(CodeBlock* block, const char* name)
		{
			TFE_Console::print("/cffffff == Disassembly Script \"%s\" ==", name);

			for (s32 offset = 0; offset < block->size;)
			{
				offset = disassembleInstruction(block, offset);
			}
		}

		s32 simpleInstruction(const char* name, s32 offset)
		{
			strcat(s_output, name);
			return offset + 1;
		}

		s32 constantInstruction(const char* name, CodeBlock* block, s32 offset)
		{
			u8 constant = block->code[offset + 1];
			char value[512];
			printValue(value, block->constants.values[constant]);
			sprintf(s_output, "%s%s %4d %s", s_output, name, constant, value);
			return offset + 2;
		}

		s32 byteInstruction(const char* name, CodeBlock* block, s32 offset)
		{
			u8 slot = block->code[offset + 1];
			sprintf(s_output, "%s%s %4d", s_output, name, slot);
			return offset + 2;
		}

		s32 jumpInstruction(const char* name, s32 sign, CodeBlock* block, s32 offset)
		{
			u16 jump = u16((block->code[offset + 1] << 8) | block->code[offset + 2]);
			sprintf(s_output, "%s%s %4d -> %d", s_output, name, offset, offset + 3 + sign*jump);
			return offset + 3;
		}

		s32 disassembleInstruction(CodeBlock* block, s32 offset)
		{
			sprintf(s_output, "%04d ", offset);

			const u8 instr = block->code[offset];
			switch (instr)
			{
			case OP_RET:
				offset = simpleInstruction("return", offset);
				break;
			case OP_POP:
				offset = simpleInstruction("pop", offset);
				break;
			case OP_DEFINE_GLOBAL:
				offset = constantInstruction("global ", block, offset);
				break;
			case OP_GET_GLOBAL:
				offset = constantInstruction("get_global", block, offset);
				break;
			case OP_SET_GLOBAL:
				offset = constantInstruction("set_global", block, offset);
				break;
			case OP_GET_LOCAL:
				offset = byteInstruction("get_local", block, offset);
				break;
			case OP_SET_LOCAL:
				offset = byteInstruction("set_local", block, offset);
				break;
			case OP_STRUCT:
				offset = constantInstruction("struct", block, offset);
				break;
			case OP_FIELD:
				offset = constantInstruction("field", block, offset);
				break;
			case OP_SET_FIELD:
				offset = constantInstruction("set_field", block, offset);
				break;
			case OP_GET_FIELD:
				offset = constantInstruction("get_field", block, offset);
				break;
			case OP_JUMP:
				offset = jumpInstruction("jump", 1, block, offset);
				break;
			case OP_LOOP:
				offset = jumpInstruction("loop", -1, block, offset);
				break;
			case OP_JUMP_IF_FALSE:
				offset = jumpInstruction("jumpIfFalse", 1, block, offset);
				break;
			case OP_JUMP_IF_TRUE:
				offset = jumpInstruction("jumpIfTrue", 1, block, offset);
				break;
			case OP_CALL:
				offset = byteInstruction("call", block, offset);
				break;
			case OP_NULL:
				offset = simpleInstruction("null", offset);
				break;
			case OP_CONSTANT:
				offset = constantInstruction("literal", block, offset);
				break;
			case OP_FALSE:
				offset = simpleInstruction("false", offset);
				break;
			case OP_TRUE:
				offset = simpleInstruction("true", offset);
				break;
			case OP_NEGATE:
				offset = simpleInstruction("negate", offset);
				break;
			case OP_NOT:
				offset = simpleInstruction("not", offset);
				break;
			case OP_INC:
				offset = simpleInstruction("inc", offset);
				break;
			case OP_DEC:
				offset = simpleInstruction("dec", offset);
				break;
				// Binary
			case OP_EQ:
				offset = simpleInstruction("eq", offset);
				break;
			case OP_NOT_EQ:
				offset = simpleInstruction("notEq", offset);
				break;
			case OP_GREATER:
				offset = simpleInstruction("greater", offset);
				break;
			case OP_LESS:
				offset = simpleInstruction("less", offset);
				break;
			case OP_GREATER_EQ:
				offset = simpleInstruction("greaterEq", offset);
				break;
			case OP_LESS_EQ:
				offset = simpleInstruction("lessEq", offset);
				break;
			case OP_ADD:
				offset = simpleInstruction("add", offset);
				break;
			case OP_SUB:
				offset = simpleInstruction("sub", offset);
				break;
			case OP_MUL:
				offset = simpleInstruction("mul", offset);
				break;
			case OP_DIV:
				offset = simpleInstruction("div", offset);
				break;
			case OP_PRINT:
				offset = simpleInstruction("print", offset);
				break;
			default:
				sprintf(s_output, "Unknown opcode %d", instr);
				offset = offset + 1;
			}

			TFE_Console::print("/c8080ff %s", s_output);
			return offset;
		}
	}
}
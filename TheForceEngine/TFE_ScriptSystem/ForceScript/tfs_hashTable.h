#pragma once
//////////////////////////////////////////////////////////////////////
// The Force Engine Script System
//////////////////////////////////////////////////////////////////////

#include <TFE_System/types.h>
#include "tfs_value.h"

namespace TFE_ForceScript
{
	struct ObjString;

	struct Entry
	{
		ObjString* key;
		Value value;
	};

	struct Table
	{
		s32 count;
		s32 capacity;
		Entry* entries;
	};

	namespace TFS_HashTable
	{
		void initTable(Table* table);
		void freeTable(Table* table);

		// Set the value at 'key' and returns true if it is a new value.
		bool tableSet(Table* table, ObjString* key, Value value);
		// Get a value in the hash table and returns false if it does not exist.
		bool tableGet(Table* table, ObjString* key, Value* value);
		// Find the string in a string table.
		ObjString* tableFindString(Table* table, const char* buffer, s32 length, u32 hash);
		// Delete an item.
		bool tableDelete(Table* table, ObjString* key);

		void tableAddAll(const Table* from, Table* to);
		s32 tableGetCount(const Table* table);

		u32 hashString(const char* key, s32 length);
	}
}
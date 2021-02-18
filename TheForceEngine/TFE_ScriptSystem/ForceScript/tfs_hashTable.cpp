#include "tfs_hashTable.h"
#include "tfs_object.h"
#include <TFE_System/system.h>
#include <TFE_FrontEndUI/console.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <vector>

#define TABLE_MAX_LOAD  3 / 4
#define TABLE_GROW_RATE 2
#define TABLE_MIN_SIZE 16

namespace TFE_ForceScript
{
	namespace TFS_HashTable
	{
		void initTable(Table* table)
		{
			table->count = 0;
			table->capacity = 0;
			table->entries = nullptr;
		}

		void freeTable(Table* table)
		{
			free(table->entries);
			initTable(table);
		}

		Entry* findEntry(Entry* entries, s32 capacity, ObjString* key)
		{
			const u32 mask = capacity - 1;
			u32 index = key->hash & mask;
			Entry* tombstone = nullptr;
			for (;;)
			{
				Entry* entry = &entries[index];
				if (!entry->key)
				{
					if (isNull(entry->value))
					{
						return tombstone ? tombstone : entry;
					}
					// Record the first tombstone so it can be reused.
					else if (!tombstone)
					{
						tombstone = entry;
					}
				}
				else if (entry->key == key)
				{
					return entry;
				}
				index = (index + 1) & mask;
			}
		}

		ObjString* tableFindString(Table* table, const char* buffer, s32 length, u32 hash)
		{
			if (!table->count) { return nullptr; }

			u32 mask = table->capacity - 1;
			u32 index = hash & mask;

			for (;;)
			{
				Entry* entry = &table->entries[index];

				if (!entry->key)
				{
					if (isNull(entry->value)) { return nullptr; }
				}
				else if (entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->buffer, buffer, length) == 0)
				{
					// The entry was found.
					return entry->key;
				}

				index = (index + 1) & mask;
			}

			return nullptr;
		}

		void tableAddAll(const Table* from, Table* to)
		{
			Entry* entry = from->entries;
			for (s32 i = 0; i < from->capacity; i++, entry++)
			{
				if (entry->key)
				{
					tableSet(to, entry->key, entry->value);
				}
			}
		}

		s32 tableGetCount(const Table* table)
		{
			return table->count;
		}

		void adjustCapacity(Table* table, s32 capacity)
		{
			// Allocate a new table.
			Entry* entries = (Entry*)malloc(sizeof(Entry) * capacity);
			for (s32 i = 0; i < capacity; i++)
			{
				entries[i].key = nullptr;
				entries[i].value = { 0 };
			}

			// Rebuild the table.
			table->count = 0;
			for (s32 i = 0; i < table->capacity; i++)
			{
				Entry* entry = &table->entries[i];
				if (!entry->key) { continue; }

				Entry* dest = findEntry(entries, capacity, entry->key);
				dest->key = entry->key;
				dest->value = entry->value;
				table->count++;
			}

			// Update the table.
			free(table->entries);
			table->entries = entries;
			table->capacity = capacity;
		}

		bool tableSet(Table* table, ObjString* key, Value value)
		{
			// Grow the table if needed.
			if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
			{
				s32 capacity = table->capacity ? (table->capacity * TABLE_GROW_RATE) : (TABLE_MIN_SIZE);
				adjustCapacity(table, capacity);
			}

			// Find the entry.
			Entry* entry = findEntry(table->entries, table->capacity, key);

			bool isNewKey = !entry->key;
			if (isNewKey && isNull(entry->value))
			{
				table->count++;
			}

			// Fill in the entry
			entry->key = key;
			entry->value = value;

			// Return true if this is a new key.
			return isNewKey;
		}

		bool tableGet(Table* table, ObjString* key, Value* value)
		{
			if (table->count == 0) { return false; }

			Entry* entry = findEntry(table->entries, table->capacity, key);
			if (!entry->key) { return false; }

			*value = entry->value;
			return true;
		}

		bool tableDelete(Table* table, ObjString* key)
		{
			if (table->count == 0) { return false; }

			// Find the entry.
			Entry* entry = findEntry(table->entries, table->capacity, key);
			if (!entry->key) { return false; }

			// Place a "tombstone" in the entry so searches still work.
			entry->key = nullptr;
			entry->value = boolValue(true);

			return true;
		}

		// FNV-1a, TODO: change to better hash function.
		u32 hashString(const char* key, s32 length)
		{
			u32 hash = 2166136261u;
			for (s32 i = 0; i < length; i++)
			{
				hash ^= (u8)key[i];
				hash *= 16777619;
			}
			return hash;
		}
	}
}
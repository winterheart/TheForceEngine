#include <TFE_System/system.h>
#include "labArchive.h"
#include <assert.h>
#include <algorithm>
#include <cstring>

LabArchive::~LabArchive()
{
	close();
}

bool LabArchive::create(const char *archivePath)
{
	// STUB: No facilities to write Lab files yet.
	return false;
}

bool LabArchive::open(const char *archivePath)
{
	m_archiveOpen = m_file.open(archivePath, FileStream::MODE_READ);
	m_curFile = -1;
	if (!m_archiveOpen) { return false; }

	// Read the directory.
	m_file.readBuffer(&m_header, sizeof(LAB_Header_t));
	m_stringTable = new char[m_header.stringTableSize + 1];
	m_entries = new LAB_Entry_t[m_header.fileCount];

	// Read the file entries.
	m_file.readBuffer(m_entries, sizeof(LAB_Entry_t), m_header.fileCount);

	// Read string table.
	m_file.readBuffer(m_stringTable, m_header.stringTableSize);
	m_file.close();
		
	strcpy(m_archivePath, archivePath);
	
	return true;
}

void LabArchive::close()
{
	m_file.close();
	m_archiveOpen = false;
	delete[] m_entries;
	delete[] m_stringTable;
}

// File Access
bool LabArchive::openFile(const char *file)
{
	if (!m_archiveOpen) { return false; }

	m_file.open(m_archivePath, FileStream::MODE_READ);
	m_curFile = -1;

	//search for this file.
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
		{
			m_curFile = i;
			break;
		}
	}

	if (m_curFile == -1)
	{
		m_file.close();
		TFE_System::logWrite(LOG_ERROR, "GOB", "Failed to load \"%s\" from \"%s\"", file, m_archivePath);
	}
	return m_curFile > -1 ? true : false;
}

bool LabArchive::openFile(u32 index)
{
	if (index >= getFileCount()) { return false; }

	m_curFile = s32(index);
	m_file.open(m_archivePath, FileStream::MODE_READ);
	return true;
}

void LabArchive::closeFile()
{
	m_curFile = -1;
	m_file.close();
}

u32 LabArchive::getFileIndex(const char* file)
{
	if (!m_archiveOpen) { return INVALID_FILE; }
	m_curFile = -1;

	//search for this file.
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
		{
			return i;
		}
	}
	return INVALID_FILE;
}

bool LabArchive::fileExists(const char *file)
{
	if (!m_archiveOpen) { return false; }
	m_curFile = -1;

	//search for this file.
	for (u32 i = 0; i < m_header.fileCount; i++)
	{
		if (strcasecmp(file, &m_stringTable[m_entries[i].nameOffset]) == 0)
		{
			return true;
		}
	}
	return false;
}

bool LabArchive::fileExists(u32 index)
{
	if (index >= getFileCount()) { return false; }
	return true;
}

size_t LabArchive::getFileLength()
{
	if (m_curFile < 0) { return 0; }
	return getFileLength(m_curFile);
}

bool LabArchive::readFile(void *data, size_t size)
{
	if (m_curFile < 0) { return false; }
	if (size == 0) { size = m_entries[m_curFile].len; }
	const size_t sizeToRead = std::min(size, (size_t)m_entries[m_curFile].len);

	m_file.seek(m_entries[m_curFile].dataOffset);
	m_file.readBuffer(data, (u32)sizeToRead);
	return true;
}

// Directory
u32 LabArchive::getFileCount()
{
	if (!m_archiveOpen) { return 0; }
	return m_header.fileCount;
}

const char* LabArchive::getFileName(u32 index)
{
	if (!m_archiveOpen) { return nullptr; }
	return &m_stringTable[m_entries[index].nameOffset];
}

size_t LabArchive::getFileLength(u32 index)
{
	if (!m_archiveOpen) { return 0; }
	return m_entries[index].len;
}

// Edit
void LabArchive::addFile(const char* fileName, const char* filePath)
{
	// STUB: No ability to add files yet.
	assert(0);
}

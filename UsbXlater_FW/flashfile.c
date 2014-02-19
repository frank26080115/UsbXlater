#include "utilities.h"
#include "flashfile.h"
#include "crc.h"
#include <stm32fx/peripherals.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

flashfilesystem_t ffsys;

int flashfile_init()
{
	int s;

	// calling flashfile_findNvmFile will automatically set ffsys.nvm_file
	if (flashfile_findNvmFile() == 0) {
		dbg_printf(DBGMODE_ERR, "NVM file not found in flash, will attempt to default\r\n");
		s = flashfile_writeDefaultNvmFile();
		if (s != 0) {
			dbg_printf(DBGMODE_ERR, "Error, flashfile_writeDefaultNvmFile failed\r\n");
			return s;
		}
	}

	// calling flashfile_findMru will automatically set ffsys.mru
	if (flashfile_findMru() == 0) {
		dbg_printf(DBGMODE_ERR, "MRU entry not found in flash, will attempt to default\r\n");
		s = flashfile_writeMru(1);
		if (s != 0) {
			dbg_printf(DBGMODE_ERR, "Error, flashfile_writeMru failed\r\n");
			return s;
		}
	}

	memcpy(ffsys.cache, ffsys.nvm_file, sizeof(nvm_file_t));
	ffsys.cache_dirty = 0;

	return 0;
}

void flashfile_cacheFlush()
{
	if (ffsys.cache_dirty != 0) {
		dbg_printf(DBGMODE_DEBUG, "flashfile_cacheFlush, cache is dirty\r\n");
		flashfile_writeNvmFile((nvm_file_t*)ffsys.cache);
	}
	memcpy(ffsys.cache, ffsys.nvm_file, sizeof(nvm_file_t));
	ffsys.cache_dirty = 0;
}

void flashfile_sectorDump()
{
	dbg_printf(DBGMODE_DEBUG, "flashfile_sectorDump\r\n");

	uint32_t magic = version_crc() ^ FLASHFILE_MAGIC;

	dbg_printf(DBGMODE_DEBUG, "magic 0x%08X, start 0x%08X, size 0x%08X, end 0x%08X, sect 0x%08X\r\n\r\n", magic, FLASHFILE_PAGE_START, FLASHFILE_PAGE_SIZE, FLASHFILE_PAGE_END, FLASHFILE_SECTOR);

	int i, j;
	uint32_t last = 0;

	// look for first instance of blank
	// we don't want to dump too much blank data
	for (i = FLASHFILE_PAGE_END; i >= FLASHFILE_PAGE_START; i--) {
		uint8_t* p = (uint8_t*)i;
		uint8_t v = *p;
		if (v != 0xFF) {
			last = i + 1;
			break;
		}
	}

	// but at least have some output
	if (last < FLASHFILE_PAGE_START + 64) {
		last = FLASHFILE_PAGE_START + 64;
	}

	i = FLASHFILE_PAGE_START;
	int old_j = 0;
	uint8_t chksum = 0;

	// we write out the dump in Intel HEX format

	for (; i <= last;)
	{
		j = i >> 16;

		if (old_j != j && j > 0)
		{
			// if the address is really high, we need to use extended addressing
			chksum = 0;
			dbg_printf(DBGMODE_DEBUG, ":02"); // byte cnt
			chksum += 0x02;
			dbg_printf(DBGMODE_DEBUG, "0000"); // empty address
			dbg_printf(DBGMODE_DEBUG, "04"); // extended address field
			dbg_printf(DBGMODE_DEBUG, "%04X", j); // upper memory
			chksum += j & 0xFF;
			chksum += j >> 8;
			dbg_printf(DBGMODE_DEBUG, "%02X\r\n", chksum ^ 0xFF);
			old_j = j;
		}

		uint32_t cnt = FLASHFILE_PAGE_END - i + 1;
		if (cnt > 16) {
			cnt = 16;
		}

		chksum = 0;
		dbg_printf(DBGMODE_DEBUG, ":%02X", cnt); // byte cnt
		chksum += cnt;
		dbg_printf(DBGMODE_DEBUG, "%04X", i & 0xFFFF); // address
		chksum += i & 0xFF;
		chksum += (i >> 8) & 0xFF;
		dbg_printf(DBGMODE_DEBUG, "00"); // record type is data

		for (int k = 0; k < cnt; k++, i++) {
			uint8_t* p = (uint8_t*)i;
			uint8_t v = *p;
			dbg_printf(DBGMODE_DEBUG, "%02X", v);
			chksum += v;
		}
		dbg_printf(DBGMODE_DEBUG, "%02X\r\n", chksum ^ 0xFF);
	}
	dbg_printf(DBGMODE_DEBUG, ":00000001FF\r\n\r\n"); // end of file
}

nvm_file_t* flashfile_findNvmFile()
{
	uint32_t magic = FLASHFILE_MAGIC;
	//magic ^= version_crc();

	for (uint32_t i = FLASHFILE_PAGE_START; i < FLASHFILE_PAGE_END - sizeof(nvm_file_t) - sizeof(uint32_t); i++)
	{
		nvm_file_t* possibleFile = (nvm_file_t*)i;
		if (possibleFile->magic == magic)
		{
			dbg_printf(DBGMODE_DEBUG, "possible NVM file magic (0x%08X) matched at 0x%08X, ", magic, i);
			uint32_t crc = crc32_prefered((uint8_t*)i, sizeof(nvm_file_t) - sizeof(uint32_t));
			dbg_printf(DBGMODE_DEBUG, "crc calculated = 0x%08X, crc read = 0x%08X\r\n", crc, possibleFile->crc);
			if (possibleFile->crc == crc) {
				dbg_printf(DBGMODE_DEBUG, "NVM file CRC matches\r\n");
				ffsys.nvm_file = possibleFile;
				return possibleFile;
			}
		}
	}

	return 0; // null means not found
}

mru_t* flashfile_findMru()
{
	if ((uint32_t)ffsys.nvm_file < FLASHFILE_PAGE_START || (uint32_t)ffsys.nvm_file >= FLASHFILE_PAGE_END) {
		dbg_printf(DBGMODE_ERR, "Error: flashfile_findMru called without a valid nvm file location\r\n");
		return 0;
	}

	for (uint32_t i = ((uint32_t)ffsys.nvm_file) + sizeof(nvm_file_t); i < FLASHFILE_PAGE_END; i++)
	{
		// check if the entry location is all valid (FF means blank, 0 means invalid)
		char is0 = 1;
		char isFF = 1;
		for (uint32_t j = 0; j < sizeof(mru_t); j++)
		{
			uint8_t* p = (uint8_t*)(i + j);
			uint8_t v = *p;
			if (v != 0) {
				is0 = 0;
			}
			if (v != 0xFF) {
				isFF = 0;
			}
		}

		if (is0 == 0 && isFF == 0)
		{
			ffsys.mru = (mru_t*)i;
			dbg_printf(DBGMODE_DEBUG, "flashfile_findMru found entry at 0x%08X, data 0x%08X\r\n", i, *ffsys.mru);
			return ffsys.mru;
		}
	}

	return 0;
}

int flashfile_writeMru(mru_t data)
{
	FLASH_Status status;
	int retry, busyRetry;

	if (ffsys.cache_dirty != 0) {
		dbg_printf(DBGMODE_DEBUG, "flashfile_writeMru, cache is dirty\r\n");
		flashfile_writeNvmFile((nvm_file_t*)ffsys.cache);
	}

	if ((uint32_t)ffsys.mru > FLASHFILE_PAGE_END - sizeof(mru_t))
	{
		// there's no more room for another MRU entry
		// so we will wipe the sector and start from the top

		dbg_printf(DBGMODE_DEBUG, "flashfile_writeMru, no more room\r\n");

		// save the previous info
		nvm_file_t* cache = (nvm_file_t*)ffsys.cache;
		uint8_t* cacheData = (uint8_t*)cache;
		memcpy(cache, ffsys.nvm_file, sizeof(nvm_file_t));
		ffsys.cache_dirty = 0;

		// refresh magic and CRC just in case
		uint32_t magic = version_crc() ^ FLASHFILE_MAGIC;
		cache->magic = magic;
		uint32_t crc = crc32_prefered((uint8_t*)cache, sizeof(nvm_file_t) - sizeof(uint32_t));
		cache->crc = crc;

		FLASH_Unlock();
		retry = busyRetry = 0;
		do
		{
			status = FLASH_EraseSector(FLASHFILE_SECTOR, VoltageRange_3); // wipe the sector and start from the top
			if (status == FLASH_BUSY)
			{
				busyRetry++;
			}
			else if (status != FLASH_COMPLETE)
			{
				retry++; busyRetry = 0;
				dbg_printf(DBGMODE_DEBUG, "Error flashfile_writeMru unable to erase sector, status = 0x%08X on attempt %d\r\n", status, retry);
			}
		}
		while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

		if (status != FLASH_COMPLETE)
		{
			dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
			FLASH_Lock();
			return -1;
		}

		for (uint32_t i = FLASHFILE_PAGE_START, j = 0; j < sizeof(nvm_file_t); i++, j++)
		{
			retry = busyRetry = 0;
			do
			{
				status = FLASH_ProgramByte(i, cacheData[j]);
				if (status == FLASH_BUSY)
				{
					busyRetry++;
				}
				else if (status != FLASH_COMPLETE)
				{
					retry++; busyRetry = 0;
					dbg_printf(DBGMODE_ERR, "Error flashfile_writeMru unable to write address 0x%08X data 0x%02X, status = 0x%08X on attempt %d\r\n", i, cache[j], status, retry);
				}
			}
			while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

			if (status != FLASH_COMPLETE)
			{
				dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
				FLASH_Lock();
				return -2;
			}
		}
		ffsys.nvm_file = (nvm_file_t*)FLASHFILE_PAGE_START; // set the new start location of previous info

		// MRU should be placed immediately after the nvm file
		ffsys.mru = (mru_t*)(((int)ffsys.nvm_file) + sizeof(nvm_file_t));
	}
	else
	{
		// there is enough room available for another MRU entry
		FLASH_Unlock();

		if ((uint32_t)ffsys.mru > FLASHFILE_PAGE_START && (uint32_t)ffsys.mru <= FLASHFILE_PAGE_END)
		{
			dbg_printf(DBGMODE_DEBUG, "flashfile_writeMru clearing old MRU at addr 0x%08X\r\n", ffsys.mru);

			retry = busyRetry = 0;
			do
			{
				// destroy the previous MRU entry
				if (sizeof(mru_t) == sizeof(uint8_t)) {
					status = FLASH_ProgramByte((uint32_t)ffsys.mru, 0);
				}
				else if (sizeof(mru_t) == sizeof(uint16_t)) {
					status = FLASH_ProgramHalfWord((uint32_t)ffsys.mru, 0);
				}
				else if (sizeof(mru_t) == sizeof(uint32_t)) {
					status = FLASH_ProgramWord((uint32_t)ffsys.mru, 0);
				}
				else if (sizeof(mru_t) == sizeof(uint64_t)) {
					status = FLASH_ProgramDoubleWord((uint32_t)ffsys.mru, 0);
				}

				if (status == FLASH_BUSY)
				{
					busyRetry++;
				}
				else if (status != FLASH_COMPLETE)
				{
					retry++; busyRetry = 0;
					dbg_printf(DBGMODE_ERR, "Error flashfile_writeMru unable to destroy MRU at address 0x%08X, status = 0x%08X on attempt %d\r\n", (uint32_t)ffsys.mru, status, retry);
				}
			}
			while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

			if (status != FLASH_COMPLETE)
			{
				dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
				FLASH_Lock();
				return -3;
			}

			ffsys.mru = &(ffsys.mru[1]); // update address to next slot
		}
		else
		{
			// MRU should be placed immediately after the nvm file
			ffsys.mru = (mru_t*)(((int)ffsys.nvm_file) + sizeof(nvm_file_t));
		}
	}

	dbg_printf(DBGMODE_DEBUG, "flashfile_writeMru writing to addr 0x%08X data 0x%08X\r\n", ffsys.mru, data);

	retry = busyRetry = 0;
	do
	{
		if (sizeof(mru_t) == sizeof(uint8_t)) {
			status = FLASH_ProgramByte((uint32_t)ffsys.mru, (uint8_t)data);
		}
		else if (sizeof(mru_t) == sizeof(uint16_t)) {
			status = FLASH_ProgramHalfWord((uint32_t)ffsys.mru, (uint16_t)data);
		}
		else if (sizeof(mru_t) == sizeof(uint32_t)) {
			status = FLASH_ProgramWord((uint32_t)ffsys.mru, (uint32_t)data);
		}
		else if (sizeof(mru_t) == sizeof(uint64_t)) {
			status = FLASH_ProgramDoubleWord((uint32_t)ffsys.mru, (uint64_t)data);
		}

		if (status == FLASH_BUSY)
		{
			busyRetry++;
		}
		else if (status != FLASH_COMPLETE)
		{
			retry++; busyRetry = 0;
			dbg_printf(DBGMODE_ERR, "Error flashfile_writeMru unable to write MRU at address 0x%08X data 0x%08X, status = 0x%08X on attempt %d\r\n", (uint32_t)ffsys.mru, data, status, retry);
		}
	}
	while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

	if (status != FLASH_COMPLETE)
	{
		dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
		FLASH_Lock();
		return -4;
	}

	FLASH_Lock();

	return 0;
}

int flashfile_writeNvmFile(nvm_file_t* data)
{
	FLASH_Status status;
	int retry, busyRetry;

	// ensure magic and CRC are right
	uint32_t magic = version_crc() ^ FLASHFILE_MAGIC;
	data->magic = magic;
	uint32_t crc = crc32_prefered((uint8_t*)data, sizeof(nvm_file_t) - sizeof(uint32_t));
	data->crc = crc;

	mru_t old_mru = 1;

	// find the next available space, must be blank
	uint32_t found = 0;

	if ((int)ffsys.mru >= (FLASHFILE_PAGE_START + sizeof(nvm_file_t)) && (int)ffsys.mru <= FLASHFILE_PAGE_END)
	{
		old_mru = *ffsys.mru;
		dbg_printf(DBGMODE_DEBUG, "ffsys.mru originally found at 0x%08X, value 0x%08X\r\n", ffsys.mru, old_mru);

		for (uint32_t i = (int)ffsys.mru + (int)sizeof(mru_t); i < FLASHFILE_PAGE_END - sizeof(nvm_file_t) - sizeof(mru_t); i += sizeof(mru_t))
		{
			// check if memory region required is blank
			char isBlank = 1;
			for (uint32_t j = 0; j < sizeof(nvm_file_t) + sizeof(mru_t); j++)
			{
				uint8_t* p = (uint8_t*)(i + j);
				if ((*p) != 0xFF) {
					isBlank = 0;
					break;
				}
			}

			if (isBlank != 0)
			{
				dbg_printf(DBGMODE_DEBUG, "flashfile_writeNvmFile found blank region at 0x%08X\r\n", i);
				found = i;
				break;
			}
		}
	}
	else
	{
		dbg_printf(DBGMODE_DEBUG, "ffsys.mru not found in range, 0x%08X\r\n", ffsys.mru);
	}

	FLASH_Unlock();

	uint8_t* p;

	if (found == 0)
	{
		// we need to erase and relocate to the top of the sector
		dbg_printf(DBGMODE_DEBUG, "flashfile_writeNvmFile starting from sector top\r\n");

		retry = busyRetry = 0;
		do
		{
			status = FLASH_EraseSector(FLASHFILE_SECTOR, VoltageRange_3);
			if (status == FLASH_BUSY)
			{
				busyRetry++;
			}
			else if (status != FLASH_COMPLETE)
			{
				retry++; busyRetry = 0;
				dbg_printf(DBGMODE_ERR, "Error flashfile_writeNvmFile unable to erase sector, status = 0x%08X on attempt %d\r\n", status, retry);
			}
		}
		while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

		if (status != FLASH_COMPLETE)
		{
			dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
			FLASH_Lock();
			return -1;
		}

		found = FLASHFILE_PAGE_START;
	}
	else
	{
		// we destroy the magic word of the previous file to make it invalid
		dbg_printf(DBGMODE_DEBUG, "flashfile_writeNvmFile destroying old NVM at addr 0x%08X\r\n", (uint32_t)ffsys.nvm_file);
		retry = busyRetry = 0;
		do
		{
			status = FLASH_ProgramWord((uint32_t)ffsys.nvm_file, 0);
			if (status == FLASH_BUSY)
			{
				busyRetry++;
			}
			else if (status != FLASH_COMPLETE)
			{
				retry++; busyRetry = 0;
				dbg_printf(DBGMODE_ERR, "Error flashfile_writeNvmFile unable to destroy magic at address 0x%08X, status = 0x%08X on attempt %d\r\n", (uint32_t)ffsys.nvm_file, status, retry);
			}
		}
		while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);
	}

	p = (uint8_t*)data;
	for (uint32_t i = found, j = 0; j < sizeof(nvm_file_t); i++, j++)
	{
		retry = busyRetry = 0;
		do
		{
			status = FLASH_ProgramByte(i, p[j]);
			if (status == FLASH_BUSY)
			{
				busyRetry++;
			}
			else if (status != FLASH_COMPLETE)
			{
				retry++; busyRetry = 0;
				dbg_printf(DBGMODE_ERR, "Error flashfile_writeNvmFile unable to write address 0x%08X data 0x%02X, status = 0x%08X on attempt %d\r\n", i, p[j], status, retry);
			}
		}
		while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

		if (status != FLASH_COMPLETE)
		{
			dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
			FLASH_Lock();
			return -2;
		}
	}

	ffsys.nvm_file = (nvm_file_t*)found;
	ffsys.mru = (mru_t*)(((void*)ffsys.nvm_file) + sizeof(nvm_file_t)); // MRU should be placed immediately after the nvm file

	retry = busyRetry = 0;
	status = FLASH_BUSY;
	do
	{
		if (sizeof(mru_t) == sizeof(uint8_t)) {
			status = FLASH_ProgramByte((uint32_t)ffsys.mru, (uint8_t)old_mru);
		}
		else if (sizeof(mru_t) == sizeof(uint16_t)) {
			status = FLASH_ProgramHalfWord((uint32_t)ffsys.mru, (uint16_t)old_mru);
		}
		else if (sizeof(mru_t) == sizeof(uint32_t)) {
			status = FLASH_ProgramWord((uint32_t)ffsys.mru, (uint32_t)old_mru);
		}
		else if (sizeof(mru_t) == sizeof(uint64_t)) {
			status = FLASH_ProgramDoubleWord((uint32_t)ffsys.mru, (uint64_t)old_mru);
		}

		if (status == FLASH_BUSY)
		{
			busyRetry++;
		}
		else if (status != FLASH_COMPLETE)
		{
			retry++;
			dbg_printf(DBGMODE_ERR, "Error flashfile_writeNvmFile unable to write MRU at address 0x%08X data 0x%08X, status = 0x%08X on attempt %d\r\n", (uint32_t)ffsys.mru, old_mru, status, retry);
		}
	}
	while (retry < FLASHFILE_RETRY_MAX && busyRetry < FLASHFILE_BUSY_MAX && status != FLASH_COMPLETE);

	if (status != FLASH_COMPLETE)
	{
		dbg_printf(DBGMODE_ERR, "Error, flash op timeout, F " __FILE__ ", L %d, s 0x%08X, R %d, BR %d\r\n", __LINE__, status, retry, busyRetry);
		FLASH_Lock();
		return -3;
	}

	FLASH_Lock();

	return 0;
}

int flashfile_writeDefaultNvmFile()
{
	nvm_file_t* data = (nvm_file_t*)ffsys.cache;

	// ignore magic and crc, these are written later inside flashfile_writeNvmFile

	// TODO

	int status = flashfile_writeNvmFile(data);

	return status;
}

void flashfile_updateEntry(void* dest, void* src, uint8_t len, char now)
{
	if (memcmp(dest, src, len) == 0)
		return;

	uint32_t offset = (uint32_t)dest - (uint32_t)ffsys.nvm_file;
	uint8_t* cache = (uint8_t*)ffsys.cache; // 32 bit to 8 bit conversion
	memcpy(&cache[offset], src, len);
	ffsys.cache_dirty = 1;
	if (now) flashfile_cacheFlush();
}

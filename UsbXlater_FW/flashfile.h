#ifndef FLASHFILE_H_
#define FLASHFILE_H_

/*

nvm means non-volatile memory

the nvm_file_t structure contains information that must be persistant even after powerdown

flash can only be erased on entire sector at once

instead of overwriting memory, we invalidate old memory and place new memory in the next available space
this is a form of flash wear leveling, to prolong the life of the flash memory

valid memory is found by matching a magic number and checking a CRC

if no more room is left in the sector, the sector is erased and the writing starts at the sector top

MRU is "most recently used", so if you switch modes, the next bootup will go into the same mode
MRU needs to be written really fast, so it's size is small and does not require magic or CRC validation
it is stored in the region immediately after the file's region

MRU data is only valid if it is not all 0s (this is how we invalidate a MRU entry) and not all 1s (flash is all 1s after a erase)

user config such as key bindings and such are handled by another flash sector in another code module!

*/

typedef uint8_t mru_t;

typedef struct
{
	uint32_t magic;
	union {
		uint32_t raw[64];
		struct {
			uint8_t dongle_bdaddr[6];
			uint8_t report_a3[8 * 6];
			uint8_t report_12[3 * 5]; // the first 6 bytes is the DS4 BD_ADDR
			uint8_t report_13[6 + 8 + 8]; // the first 6 bytes is the PS4 BD_ADDR
			uint8_t report_02[36];
			uint8_t link_key[16];
		} fmt;
	} d;
	uint32_t crc;
}
nvm_file_t;

typedef struct
{
	nvm_file_t* nvm_file;
	mru_t* mru;
	uint32_t cache[64 + 2];
	char cache_dirty;
}
flashfilesystem_t;

#define FLASHFILE_MAGIC 0x1234ABCD
#define FLASHFILE_PAGE_START 0x080C0000
#define FLASHFILE_PAGE_SIZE 0x00020000
#define FLASHFILE_PAGE_END (FLASHFILE_PAGE_START + FLASHFILE_PAGE_SIZE - 1)
#define FLASHFILE_SECTOR FLASH_Sector_10

#define FLASHFILE_RETRY_MAX 8
#define FLASHFILE_BUSY_MAX 256

extern flashfilesystem_t flashfilesystem;

int flashfile_init();
void flashfile_sectorDump();
void flashfile_cacheFlush();
nvm_file_t* flashfile_findNvmFile();
mru_t* flashfile_findMru();
int flashfile_writeMru(mru_t data);
int flashfile_writeNvmFile(nvm_file_t* data);
int flashfile_writeDefaultNvmFile();

#endif

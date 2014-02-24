#ifndef FLASHFILE_H_
#define FLASHFILE_H_

#include <btstack/utils.h>
#include <ds4_emu.h>
#include <usbd_dev/usbd_dev_ds4.h>

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

typedef uint32_t mru_t;

typedef struct
{
	uint32_t magic;
	union {
		uint32_t raw[64 - 16];
		struct {
			uint8_t report_A3[DS4_REPORT_A3_LEN];
			uint8_t report_02[DS4_REPORT_02_LEN];
			uint16_t ds4_vid;
			uint16_t ds4_pid;
			uint8_t ds4_bdaddr[BD_ADDR_LEN];
			uint8_t bt_bdaddr[BD_ADDR_LEN];
			uint8_t ps4_bdaddr[BD_ADDR_LEN];
			uint8_t ps4_link_key[LINK_KEY_LEN];
			uint32_t ps4_conn_crc;
		} fmt;
	} d;
	uint32_t crc;
}
nvm_file_t;

typedef struct
{
	nvm_file_t* nvm_file;
	mru_t* mru;
	uint32_t cache[64 - 16 + 4];
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

extern flashfilesystem_t ffsys;

int flashfile_init();
void flashfile_sectorDump();
void flashfile_cacheFlush();
nvm_file_t* flashfile_findNvmFile();
mru_t* flashfile_findMru();
int flashfile_writeMru(mru_t data);
int flashfile_writeNvmFile(nvm_file_t* data);
int flashfile_writeDefaultNvmFile();
void flashfile_updateEntry(void* dest, void* src, uint8_t len, char now);

#endif

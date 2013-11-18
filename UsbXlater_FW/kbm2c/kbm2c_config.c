#include "kbm2ctrl.h"
#include "kbm2ctrl_keycodes.h"
#include <stm32f2xx.h>
#include <stm32f2xx_flash.h>

#define KBM2C_CONFIG_SECTOR FLASH_Sector_11
#define KBM2C_FLASH_START   0x080E0000
#define KBM2C_FLASH_END     0x080FFFFF
#define KBM2C_FLASH_SIZE    (KBM2C_FLASH_END - KBM2C_FLASH_START + 1)
#define KBM2C_FLASH_VoltageRange VoltageRange_4
#define KBM2C_CONFIG_NUM_TO_ADDR(num) (KBM2C_FLASH_START + (sizeof(kbm2c_config_t) * (num)))

uint8_t kbm2c_stickLookUp[131] = { 0,1,2,11,16,20,23,25,28,30,32,34,36,38,39,41,43,44,45,47,48,50,51,52,53,54,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,74,75,76,77,78,79,80,80,81,82,83,83,84,85,86,87,87,88,89,89,90,91,92,92,93,94,94,95,96,96,97,98,98,99,100,100,101,102,102,103,103,104,105,105,106,107,107,108,108,109,110,110,111,111,112,112,113,114,114,115,115,116,116,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,127,127, 0, };

kbm2c_err_t kbm2c_config_init()
{
	return KBM2C_OK;
}

kbm2c_err_t kbm2c_config_default(kbm2c_config_t* dest)
{
	dest->mousemap = MOUSE_TO_RIGHT_STICK;
	dest->left_stick_deadzone = 28;
	dest->right_stick_deadzone = 28;
	dest->left_stick_max = 127;
	dest->right_stick_max = 127;
	dest->left_stick_curve[0] = 0;
	dest->right_stick_curve[0] = 0;
	for (uint16_t i = 1; i < 131; i++)
	{
		if (kbm2c_stickLookUp[i] != 0) {
			dest->left_stick_curve[i] = kbm2c_stickLookUp[i];
			dest->right_stick_curve[i] = kbm2c_stickLookUp[i];
		}
		else {
			dest->right_stick_curve[i] = dest->left_stick_max;
			dest->right_stick_curve[i] = dest->right_stick_max;
			break;
		}
	}
	dest->left_stick_gain[0] = 1.0;
	dest->left_stick_gain[1] = 16.0;
	dest->left_stick_gain[2] = 32.0;
	dest->right_stick_gain[0] = 1.0;
	dest->right_stick_gain[1] = 16.0;
	dest->right_stick_gain[2] = 32.0;
	dest->trigger_down_speed = 64;
	dest->trigger_up_speed = 64;
	dest->mouse_filter = 0.5d;
	dest->mouse_decay = 20;
	return KBM2C_OK;
}

// TODO: flash reading / writing
// TODO: diagnostics for failures

kbm2c_err_t kbm2c_config_load(uint8_t configNum, kbm2c_config_t* dest)
{
	uint32_t addr = KBM2C_CONFIG_NUM_TO_ADDR(configNum);
	uint16_t checksum = fletcher16(addr, sizeof(kbm2c_config_t) - sizeof(uint16_t));
	kbm2c_config_t* cptr = (kbm2c_config_t*)addr;
	if (cptr->checksum != checksum) {
		kbm2c_config_default(dest);
		return KBM2C_ERR_CHKSUM;
	}
	memcpy(dest, cptr, sizeof(kbm2c_config_t));
	return KBM2C_OK;
}

kbm2c_err_t kbm2c_config_erase()
{
	FLASH_Unlock();
	FLASH_EraseSector(KBM2C_CONFIG_SECTOR, KBM2C_FLASH_VoltageRange);
	FLASH_Lock();
	return KBM2C_OK;
}

kbm2c_err_t kbm2c_config_write(uint8_t configNum, kbm2c_config_t* src)
{
	uint32_t addr = KBM2C_CONFIG_NUM_TO_ADDR(configNum);

	uint8_t* fptr = (uint8_t*)addr;
	for (int i = 0; i < sizeof(kbm2c_config_t); i++)
	{
		if (fptr[i] != 0xFF) {
			return KBM2C_ERR_FLASH_NOT_EMPTY;
		}
	}

	uint16_t checksum = fletcher16(src, sizeof(kbm2c_config_t) - sizeof(uint16_t));
	src->checksum = checksum;
	uint8_t* ptr = (uint8_t*)src;
	FLASH_Unlock();
	for (int i = 0; i < sizeof(kbm2c_config_t); i++)
	{
		FLASH_ProgramByte(addr + i, ptr[i]);
	}
	FLASH_Lock();
	return KBM2C_OK;
}

kbm2c_err_t kbm2c_config_verify_raw(uint8_t* rawData)
{
	kbm2c_config_t* cptr = (kbm2c_config_t*)rawData;
	uint16_t checksum = fletcher16(rawData, sizeof(kbm2c_config_t) - sizeof(uint16_t));
	if (checksum != cptr->checksum) {
		return KBM2C_ERR_CHKSUM;
	}
	return KBM2C_OK;
}

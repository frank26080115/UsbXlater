#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utilities.h>
#include <crc.h>

char* version_string()
{
	uint8_t* buff = &global_temp_buff[(GLOBAL_TEMP_BUFF_SIZE / 2) + 16];
	int idx = 0;
	idx += sprintf(&buff[idx], "UsbXlater, ");
	idx += sprintf(&buff[idx], "Compiled on " __DATE__ ", " __TIME__", ");
	#ifdef __GNUC__
	idx += sprintf(&buff[idx], "GNU C %d", __GNUC__);
	#ifdef __GNUC_MINOR__
	idx += sprintf(&buff[idx], ".%d", __GNUC_MINOR__);
	#ifdef __GNUC_PATCHLEVEL__
	idx += sprintf(&buff[idx], ".%d", __GNUC_PATCHLEVEL__);
	#endif
	#endif
	#else
	idx += sprintf(&buff[idx], "unknown compiler\r\n");
	#endif
	return (char*)buff;
}

uint32_t version_crc()
{
	char* str = version_string();
	int len = strlen(str);
	return crc32_calc(str, len);
}

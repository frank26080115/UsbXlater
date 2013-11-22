#include "hidrpt.h"
#include <vcp.h>
#include <stdlib.h>
#include <string.h>

void HID_Rpt_Desc_Parse(uint8_t* desc, int length, HID_Rpt_Parsing_Params_t* parser, uint8_t intf, uint8_t* repIdList)
{
	int descIdx = 0;
	int repBitIdx = 0;
	int8_t usagePage = -1;
	uint16_t usage[8] = { 0, 0, 0, 0, 0, 0, 0, 0, };
	int8_t usageCnt = 0;
	int8_t stackedUsage = -1;
	int8_t stackedUsagePage = -1;
	int8_t collection = -1;
	int8_t repId = -1;
	int8_t repIdCnt = 0;
	int8_t repSize = -1;
	int8_t repCnt = -1;

	// to understand this code, you must understand the HID report descriptor specifications
	// and refer to the HID usage tables documentation

	for (descIdx = 0; descIdx < length; )
	{
		uint8_t b0 = desc[descIdx];
		descIdx++; // minimum size is 1, so increment here
		uint8_t bSize = b0 & 0x03;
		bSize = bSize == 3 ? 4 : bSize; // size is 4 when bSize is 3
		uint8_t bType = (b0 >> 2) & 0x03;
		uint8_t bTag = (b0 >> 4) & 0x0F;

		// ignore long tags

		if (bType == 0) // bType is "main"
		{
			if (bTag == 0x08)
			{
				// input

				if ((desc[descIdx] & 0x01) == 0) // must be data, not a constant
				{
					if (stackedUsagePage == 0x01 && (stackedUsage == 0x02 || stackedUsage == 0x01))
					{
						// generic desktop mouse or pointer

						parser->mouse_exists = 1;
						parser->mouse_intf = intf;
						parser->mouse_report_id = repId;

						if (usagePage == 0x09) // button
						{
							parser->mouse_but_bitidx = repBitIdx;
							usageCnt = 0;
						}
						else if (usagePage == 0x01 && usageCnt == 2 && repCnt == 2 && usage[0] == 0x30 && usage[1] == 0x31)
						{
							parser->mouse_xy_idx = repBitIdx / 8;
							parser->mouse_xy_size = repSize;
							usageCnt = 0;
						}
						else if (usagePage == 0x01 && usageCnt == 3 && repCnt == 3 && usage[0] == 0x30 && usage[1] == 0x31 && (usage[2] == 0x38 || usage[2] == 0x32))
						{
							// X Y combined with wheel
							parser->mouse_xy_idx = repBitIdx / 8;
							parser->mouse_xy_size = repSize;
							parser->mouse_wheel_size = repSize;
							parser->mouse_wheel_idx = parser->mouse_xy_idx + (repSize / 4);
							usageCnt = 0;
						}
						else if (usagePage == 0x01 && usageCnt == 1 && repCnt == 1 && usage[0] == 0x38)
						{
							// wheel is separate
							parser->mouse_wheel_idx = repBitIdx / 8;
							parser->mouse_wheel_size = repSize;
							usageCnt = 0;
						}
						else if (usagePage == 0x0C && usageCnt == 1 && repCnt == 1 && usage[0] == 0x238)
						{
							// page "consumer", usage "ac pan", this is how my logitech mouse does side scroll
							parser->mouse_wheelx_idx = repBitIdx / 8;
							parser->mouse_wheelx_size = repSize;
							usageCnt = 0;
						}
					}
					else if (stackedUsagePage == 0x01 && stackedUsage == 0x06 && usagePage == 0x07)
					{
						parser->kb_exists = 1;
						parser->kb_intf = intf;
						parser->kb_report_id = repId;
						// TODO: handle special keyboards that don't follow the standard
					}
				}

				repBitIdx += repSize * repCnt;

				if (repIdCnt < 7) {
					repIdList[repIdCnt++] = repId;
				}
			}
			else if (bTag == 0x09) {
				// output
				// ignore
			}
			else if (bTag == 0x0B) {
				// feature
				// ignore
			}
			else if (bTag == 0x0A) {
				// collection
				collection = *((int8_t*)(&(desc[descIdx])));
				stackedUsagePage = usagePage;
				if (usageCnt > 0) {
					stackedUsage = usage[0];
				}
				usageCnt = 0;
			}
			else if (bTag == 0x0C) {
				// end collection
				repId = -1;
				collection = -1;
				usageCnt = 0;
				usagePage = -1;
			}
		}
		else if (bType == 0x01) // bType is "global"
		{
			if (bTag == 0x00) {
				// usage page
				usagePage = *((int8_t*)(&(desc[descIdx])));
			}
			else if (bTag == 0x07) {
				// report size
				repSize = *((int8_t*)(&(desc[descIdx])));
			}
			else if (bTag == 0x08) {
				// report ID
				repId = *((int8_t*)(&(desc[descIdx])));
			}
			else if (bTag == 0x09) {
				// report count
				repCnt = *((int8_t*)(&(desc[descIdx])));
			}
			// other bTags ignored
		}
		else if (bType == 0x02) // bType is "local"
		{
			if (bTag == 0x00) {
				// usage
				if (usageCnt < 8)
				{
					usage[usageCnt] = *((uint16_t*)(&(desc[descIdx])));
					if (bSize == 1) {
						usage[usageCnt] &= 0xFF;
					}
					usageCnt++;
				}
			}
			// other bTags ignored
		}

		descIdx += bSize;
	}
}

void HID_Rpt_Parsing_Params_Reset(HID_Rpt_Parsing_Params_t* x)
{
	memset(x, 0xFF, sizeof(HID_Rpt_Parsing_Params_t));
}

void HID_Rep_Parsing_Params_Debug_Dump(HID_Rpt_Parsing_Params_t* x)
{
	uint8_t* xp = (uint8_t*)x;
	dbg_printf(DBGMODE_DEBUG, "Parsing_Params Dump: ");
	vcp_printf("Parsing_Params Dump: ");
	for (int i = 0; i < sizeof(HID_Rpt_Parsing_Params_t); i++) {
		dbg_printf(DBGMODE_DEBUG, "0x%02X ", xp[i]);
		vcp_printf("0x%02X ", xp[i]);
	}
	dbg_printf(DBGMODE_DEBUG, "\r\n");
	vcp_printf("\r\n");
}

#include "hidrpt.h"
#include <stdlib.h>
#include <string.h>

void HID_Rpt_Desc_Parse(uint8_t* desc, int length, HID_Rpt_Parsing_Params_t* parser, uint8_t intf)
{
	int idx = 0;
	int8_t usage = 0;
	int8_t usagePage = 0;
	int8_t collection = -1;
	int8_t repId = -1;
	int8_t repCnt = -1;
	Usage_Type_t curUsage = USAGE_NONE;
	parser->max_repId = 0;

	for (idx = 0; idx < length; )
	{
		uint8_t b0 = desc[idx++];
		uint8_t bSize = b0 & 0x03;
		bSize = bSize == 3 ? 4 : bSize; // size is 4 when bSize is 3
		uint8_t bType = (b0 >> 2) & 0x03;
		uint8_t bTag = (b0 >> 4) & 0x0F;

		// ignore long tags

		if (bType == 0)
		{
			if (bTag == 0x08)
			{
				// input
				if (curUsage == USAGE_MOUSE)
				{
					parser->mouse_exists = 1;
					parser->mouse_intf = intf;
					parser->mouse_x_size = 1;
					parser->mouse_y_size = 1;
					parser->mouse_wheel_size = 1;

					parser->mouse_but_left_byteidx = 0;
					parser->mouse_but_mid_byteidx = 0;
					parser->mouse_but_right_byteidx = 0;
					parser->mouse_but_left_bitidx = 0;
					parser->mouse_but_mid_bitidx = 1;
					parser->mouse_but_right_bitidx = 2;
					parser->mouse_x_idx = 1;
					parser->mouse_y_idx = 2;
					parser->mouse_wheel_idx = 3;
					if (repId >= 0)
					{
						parser->mouse_report_id = repId;
						parser->mouse_but_left_byteidx++;
						parser->mouse_but_mid_byteidx++;
						parser->mouse_but_right_byteidx++;
						parser->mouse_x_idx++;
						parser->mouse_y_idx++;
						parser->mouse_wheel_idx++;
					}
				}
				else if (curUsage == USAGE_KBRD)
				{
					parser->kb_exists = 1;
					parser->kb_intf = intf;
					parser->kb_modifier_idx = 0;
					parser->kb_keycode_idx = 2;
					parser->kb_keycode_cnt = 6;
					if (repId >= 0)
					{
						parser->kb_report_id = repId;
						parser->kb_modifier_idx++;
						parser->kb_keycode_idx++;
						parser->kb_keycode_cnt = 5;
					}
				}
				else if (curUsage == USAGE_CONSUMER)
				{
					parser->consumer_exists = 1;
					parser->consumer_intf = intf;
					parser->consumer_code_idx = 0;
					if (repId >= 0)
					{
						parser->consumer_report_id = repId;
						parser->consumer_code_idx++;
					}
				}
				else if (curUsage == USAGE_SYSCTRL)
				{
					parser->sysctrl_exists = 1;
					parser->sysctrl_intf = intf;
					parser->sysctrl_code_idx = 0;
					if (repId >= 0)
					{
						parser->sysctrl_report_id = repId;
						parser->sysctrl_code_idx++;
					}
				}
			}
			else if (bTag == 0x09) {
				// output
			}
			else if (bTag == 0x0B) {
				// feature
			}
			else if (bTag == 0x0A) {
				// collection
				collection = *((int8_t*)(&(desc[idx])));
			}
			else if (bTag == 0x0C) {
				// end collection
				repId = -1;
				collection = -1;
				curUsage = USAGE_NONE;
			}
		}
		else if (bType == 0x01)
		{
			if (bTag == 0x00) {
				// usage page
				usagePage = *((int8_t*)(&(desc[idx])));
				if (usagePage == 0x07) {
					curUsage = USAGE_KBRD;
				}
			}
			else if (bTag == 0x08) {
				// report ID
				repId = *((int8_t*)(&(desc[idx])));
				if (repId > parser->max_repId) parser->max_repId = repId;
			}
			else if (bTag == 0x09) {
				// report count
				repCnt = *((int8_t*)(&(desc[idx])));
			}
			// other bTags ignored
		}
		else if (bType == 0x02)
		{
			if (bTag == 0x00) {
				// usage
				usage = *((int8_t*)(&(desc[idx])));
				if ((usage == 0x02 || usage == 0x01) && usagePage == 0x01) {
					curUsage = USAGE_MOUSE;
				}
				else if ((usage == 0x07 && usagePage == 0x01) || usagePage == 0x07) {
					curUsage = USAGE_KBRD;
				}
				else if (usagePage == 0x0C) {
					curUsage = USAGE_CONSUMER;
				}
				else if (usage == 0x80 && usagePage == 0x01) {
					curUsage = USAGE_SYSCTRL;
				}
			}
			// other bTags ignored
		}

		idx += bSize;
	}
}

void HID_Rpt_Parsing_Params_Reset(HID_Rpt_Parsing_Params_t* x)
{
	memset(x, 0xFF, sizeof(HID_Rpt_Parsing_Params_t));
}

uint16_t HID_Rpt_Desc_Parse_Simple(uint8_t* desc, int length)
{
	int idx = 0;
	int8_t usage = 0;
	int8_t usagePage = 0;
	int8_t collection = -1;
	int8_t repId = -1;
	int8_t repCnt = -1;
	Usage_Type_t curUsage = USAGE_NONE;

	for (idx = 0; idx < length; )
	{
		uint8_t b0 = desc[idx++];
		uint8_t bSize = b0 & 0x03;
		bSize = bSize == 3 ? 4 : bSize; // size is 4 when bSize is 3
		uint8_t bType = (b0 >> 2) & 0x03;
		uint8_t bTag = (b0 >> 4) & 0x0F;

		// ignore long tags

		if (bType == 0)
		{
			if (bTag == 0x08)
			{
				// input
				if (curUsage == USAGE_MOUSE)
				{
					if (repId >= 0)
					{
						return (uint16_t)1 | ((uint16_t)repId << 8);
					}
					else
					{
						return 1;
					}
				}
				else if (curUsage == USAGE_KBRD)
				{
					if (repId >= 0)
					{
						return (uint16_t)2 | ((uint16_t)repId << 8);
					}
					else
					{
						return 2;
					}
				}
				else if (curUsage == USAGE_CONSUMER)
				{
					return 0;
				}
				else if (curUsage == USAGE_SYSCTRL)
				{
					return 0;
				}
			}
			else if (bTag == 0x09) {
				// output
			}
			else if (bTag == 0x0B) {
				// feature
			}
			else if (bTag == 0x0A) {
				// collection
				collection = *((int8_t*)(&(desc[idx])));
			}
			else if (bTag == 0x0C) {
				// end collection
				repId = -1;
				collection = -1;
				curUsage = USAGE_NONE;
			}
		}
		else if (bType == 0x01)
		{
			if (bTag == 0x00) {
				// usage page
				usagePage = *((int8_t*)(&(desc[idx])));
				if (usagePage == 0x07) {
					curUsage = USAGE_KBRD;
				}
			}
			else if (bTag == 0x08) {
				// report ID
				repId = *((int8_t*)(&(desc[idx])));
			}
			else if (bTag == 0x09) {
				// report count
				repCnt = *((int8_t*)(&(desc[idx])));
			}
			// other bTags ignored
		}
		else if (bType == 0x02)
		{
			if (bTag == 0x00) {
				// usage
				usage = *((int8_t*)(&(desc[idx])));
				if ((usage == 0x02 || usage == 0x01) && usagePage == 0x01) {
					curUsage = USAGE_MOUSE;
				}
				else if ((usage == 0x07 && usagePage == 0x01) || usagePage == 0x07) {
					curUsage = USAGE_KBRD;
				}
				else if (usagePage == 0x0C) {
					curUsage = USAGE_CONSUMER;
				}
				else if (usage == 0x80 && usagePage == 0x01) {
					curUsage = USAGE_SYSCTRL;
				}
			}
			// other bTags ignored
		}

		idx += bSize;
	}

	return 0;
}

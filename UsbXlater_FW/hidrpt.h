#ifndef HIDRPT_H_
#define HIDRPT_H_

#include <stdint.h>

typedef struct {
	int8_t mouse_exists; // must be > 0 to be true
	uint8_t mouse_ep;
	int8_t mouse_report_id; // negative if not used, or else it must match
	int8_t mouse_xy_idx;
	int8_t mouse_xy_size;
	int8_t mouse_wheel_idx;
	int8_t mouse_wheel_size;
	int8_t mouse_wheelx_idx;
	int8_t mouse_wheelx_size;
	int8_t mouse_but_bitidx;
	int8_t kb_exists; // must be > 0 to be true
	uint8_t kb_ep;
	int8_t kb_report_id; // negative if not used, or else it must match
	int8_t kb_modifier_idx;
	int8_t kb_keycode_idx;
	int8_t kb_keycode_cnt;

	// TODO: maybe add support for consumer and sysctrl keys
	int8_t consumer_exists; // must be > 0 to be true
	uint8_t consumer_ep;
	int8_t consumer_report_id;
	int8_t consumer_code_idx;
	int8_t sysctrl_exists; // must be > 0 to be true
	uint8_t sysctrl_ep;
	int8_t sysctrl_report_id;
	int8_t sysctrl_code_idx;
}
HID_Rpt_Parsing_Params_t;

void HID_Rpt_Desc_Parse(uint8_t* desc, int length, HID_Rpt_Parsing_Params_t* parser, uint8_t ep, uint8_t* repIdList);
void HID_Rpt_Parsing_Params_Reset(HID_Rpt_Parsing_Params_t* desc);
void HID_Rpt_Parsing_Params_Debug_Dump(HID_Rpt_Parsing_Params_t* desc);

#endif

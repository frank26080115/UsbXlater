#ifndef HIDRPT_H_
#define HIDRPT_H_

#include <stdint.h>

typedef struct {
	int8_t max_repId;
	int8_t mouse_exists; // must be > 0 to be true
	int8_t mouse_intf; // interface number, negative if not used
	int8_t mouse_report_id; // negative if not used, or else it must match
	int8_t mouse_x_idx;
	int8_t mouse_x_size; // 1, 2, or 4
	int8_t mouse_y_idx;
	int8_t mouse_y_size; // 1, 2, or 4
	int8_t mouse_wheel_idx;
	int8_t mouse_wheel_size; // 1, 2, or 4
	int8_t mouse_wheelx_idx; // z is wheel
	int8_t mouse_wheelx_size; // 1, 2, or 4
	int8_t mouse_but_left_bitidx; // 0-7
	int8_t mouse_but_left_byteidx;
	int8_t mouse_but_mid_bitidx; // 0-7
	int8_t mouse_but_mid_byteidx;
	int8_t mouse_but_right_bitidx; // 0-7
	int8_t mouse_but_right_byteidx;
	int8_t mouse_wheel_left_bitidx; // 0-7
	int8_t mouse_wheel_left_byteidx;
	int8_t mouse_wheel_right_bitidx; // 0-7
	int8_t mouse_wheel_right_byteidx;
	int8_t mouse_but_forward_bitidx; // 0-7
	int8_t mouse_but_forward_byteidx;
	int8_t mouse_but_backward_bitidx; // 0-7
	int8_t mouse_but_backward_byteidx;
	int8_t kb_exists; // must be > 0 to be true
	int8_t kb_intf; // interface number, negative if not used
	int8_t kb_report_id; // negative if not used, or else it must match
	int8_t kb_modifier_idx;
	int8_t kb_keycode_idx;
	int8_t kb_keycode_cnt;
	int8_t consumer_exists; // must be > 0 to be true
	int8_t consumer_intf; // interface number, negative if not used
	int8_t consumer_report_id;
	int8_t consumer_code_idx;
	int8_t sysctrl_exists; // must be > 0 to be true
	int8_t sysctrl_intf; // interface number, negative if not used
	int8_t sysctrl_report_id;
	int8_t sysctrl_code_idx;
}
HID_Rpt_Parsing_Params_t;

typedef enum {
	USAGE_NONE,
	USAGE_MOUSE,
	USAGE_KBRD,
	USAGE_CONSUMER,
	USAGE_SYSCTRL,
}
Usage_Type_t;

void HID_Rpt_Desc_Parse(uint8_t* desc, int length, HID_Rpt_Parsing_Params_t* parser, uint8_t intf);
uint16_t HID_Rpt_Desc_Parse_Simple(uint8_t* desc, int length);
void HID_Rpt_Parsing_Params_Reset(HID_Rpt_Parsing_Params_t* x);

#endif
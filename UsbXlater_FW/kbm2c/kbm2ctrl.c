/*
 * UsbXlater - by Frank Zhao
 *
 * This is the layer that translates between raw data from the mouse to raw data sent to a Playstation
 *
 * Everything is hardcoded for now
 *
 * The mouse data input is very specific to my Logitech mouse, it has a weird 12 bit report
 * other mouse might not use the same format and thus will not work
 * but it's easy to rewrite it to work with other mice
 *
 * The keymappings are done specifically for Battlefield 4 right now
 * I am working on making remapping and reconfiguring easier
 *
 */

#include "kbm2ctrl.h"
#include "kbm2ctrl_keycodes.h"
#include "hidrpt.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <usbotg_lib/usb_core.h>
#include <usbd_dev/usbd_dev_ds3.h>
#include <usbh_dev/usbh_dev_dualshock.h>

extern USB_OTG_CORE_HANDLE USB_OTG_Core_dev;

uint32_t kbm2c_keyFlags[KBM2C_KEYFLAGS_SIZE];
uint32_t kbm2c_keyFlagsPrev[KBM2C_KEYFLAGS_SIZE];
mouse_data_t kbm2c_mouseData;
int8_t kbm2c_randomDeadZone[4] = { 0, 0, 0, 0, };
volatile uint32_t kbm2c_mouseTimestamp;
kbm2c_config_t kbm2c_config;
ctrler_data_t ctrler_data;
ds3_packet_t ds3_packet;
ds4_packet_t ds4_packet;
int8_t kbm2c_leftGainIdx = 0;
int8_t kbm2c_rightGainIdx = 0;

// special testing utilities
int16_t kbm2c_testOffset = 0;

void kbm2c_init()
{
	// TODO: advanced load
	kbm2c_config_default(&kbm2c_config);
}

void kbm2c_handleKeyReport(uint8_t modifier, uint8_t* data, uint8_t len)
{
	led_1_tog();

	memset(kbm2c_keyFlags, 0, KBM2C_KEYFLAGS_SIZE * sizeof(uint32_t));
	for (uint8_t i = 0; i < len; i++)
	{
		if (data[i] != 0)
		{
			kbm2c_keyFlags[data[i] / 32] |= (1UL << (data[i] % 32));
		}

		if (data[i] == KEYCODE_F11) {
			kbm2c_testOffset += 10;
			cereal_printf("kbm2c_testOffset %d \r\n", kbm2c_testOffset);
		}
		else if (data[i] == KEYCODE_F10) {
			kbm2c_testOffset += 1;
			cereal_printf("kbm2c_testOffset %d \r\n", kbm2c_testOffset);
		}
		else if (data[i] == KEYCODE_F9) {
			kbm2c_testOffset -= 1;
			cereal_printf("kbm2c_testOffset %d \r\n", kbm2c_testOffset);
		}
		else if (data[i] == KEYCODE_F8) {
			kbm2c_testOffset -= 10;
			cereal_printf("kbm2c_testOffset %d \r\n", kbm2c_testOffset);
		}
	}
	kbm2c_keyFlags[KBM2C_KEYFLAGS_SIZE - 1] |= modifier;
}

void kbm2c_handleMouseReport(uint8_t* data, uint8_t len, HID_Rpt_Parsing_Params_t* parser)
{
	led_1_tog();

	kbm2c_mouseTimestamp = systick_1ms_cnt;

	int32_t x32, y32;

	if (parser->mouse_xy_size == 8)
	{
		int8_t x8 = (int8_t)data[parser->mouse_xy_idx];
		int8_t y8 = (int8_t)data[parser->mouse_xy_idx + 1];
		x32 = x8;
		y32 = y8;
		if (x8 < 0 && x32 > 0) x32 *= -1;
		if (y8 < 0 && y32 > 0) y32 *= -1;
	}
	else if (parser->mouse_xy_size == 12)
	{
		uint16_t xu16 = data[parser->mouse_xy_idx];
		xu16 += (data[parser->mouse_xy_idx + 1] & 0x0F) << 8;
		if ((data[parser->mouse_xy_idx + 1] & 0x08) != 0) xu16 |= 0xF000;

		int16_t x16 = (int16_t)xu16;
		x32 = x16;
		if (x16 < 0 && x32 > 0) x32 *= -1;

		uint16_t yu = data[parser->mouse_xy_idx + 2] << 8;
		yu += data[parser->mouse_xy_idx + 1] & 0xF0;
		int16_t y16 = (int16_t)yu;
		y16 /= 16;
		y32 = y16;
		if (y16 < 0 && y32 > 0) y32 *= -1;
	}
	else if (parser->mouse_xy_size == 16)
	{
		int16_t x16 = *((int16_t*)(&data[parser->mouse_xy_idx]));
		int16_t y16 = *((int16_t*)(&data[parser->mouse_xy_idx + 2]));
		x32 = x16;
		y32 = y16;
		if (x16 < 0 && x32 > 0) x32 *= -1;
		if (y16 < 0 && y32 > 0) y32 *= -1;
	}
	// TODO: add 20, 24, 28 bits?
	else if (parser->mouse_xy_size == 32)
	{
		int32_t x_ = *((int32_t*)(&data[parser->mouse_xy_idx]));
		int32_t y_ = *((int32_t*)(&data[parser->mouse_xy_idx + 4]));
		x32 = x_;
		y32 = y_;
		if (x_ < 0 && x32 > 0) x32 *= -1;
		if (y_ < 0 && y32 > 0) y32 *= -1;
	}

	int8_t w = 0;
	w = (int8_t)data[parser->mouse_wheel_idx];
	if ((w > 0 && kbm2c_mouseData.w < 0) || (w < 0 && kbm2c_mouseData.w > 0)) kbm2c_mouseData.w = 0;
	kbm2c_mouseData.w += w * 6;
	// TODO: take wheel size into account?

	// TODO: add side click/x wheel?
	//int8_t wx = 0;
	//wx = (int8_t)data[parser->mouse_wheelx_idx];
	//if ((wx > 0 && kbm2c_mouseData.wx < 0) || (wx < 0 && kbm2c_mouseData.wx > 0)) kbm2c_mouseData.wx = 0;
	//kbm2c_mouseData.wx += w * 6;

	double xd = x32;
	double yd = y32;

	kbm2c_mouseData.x = (xd * kbm2c_config.mouse_filter) + (kbm2c_mouseData.x * (1.0d - kbm2c_config.mouse_filter));
	kbm2c_mouseData.y = (yd * kbm2c_config.mouse_filter) + (kbm2c_mouseData.y * (1.0d - kbm2c_config.mouse_filter));

	kbm2c_mouseData.btn = data[parser->mouse_but_bitidx / 8];
	// TODO: add more than 8 buttons?
}

void kbm2c_handleDs3Report(uint8_t* data)
{
	kbm2c_task(1);
}

void kbm2c_handleDs4Report(uint8_t* data)
{
	kbm2c_task(1);
}

void kbm2c_task(char force)
{
	static uint32_t lastReportTime;
	uint32_t nowTime = systick_1ms_cnt;
	if ((nowTime - lastReportTime) >= 10 || force != 0)
	{
		lastReportTime = nowTime;
		if ((nowTime - kbm2c_mouseTimestamp) > 21)
		{
			kbm2c_mouseTimestamp = nowTime;
			kbm2c_mouseData.x = 0;
			kbm2c_mouseData.y = 0;
		}
		else if ((nowTime - kbm2c_mouseTimestamp) > 13)
		{
			kbm2c_mouseTimestamp = nowTime;
			kbm2c_mouseData.x = (kbm2c_mouseData.x * (1.0d - kbm2c_config.mouse_filter));
			kbm2c_mouseData.y = (kbm2c_mouseData.y * (1.0d - kbm2c_config.mouse_filter));
			if (kbm2c_mouseData.x > kbm2c_config.mouse_decay) kbm2c_mouseData.x -= kbm2c_config.mouse_decay;
			else if (kbm2c_mouseData.x < -kbm2c_config.mouse_decay) kbm2c_mouseData.x += kbm2c_config.mouse_decay;
			if (kbm2c_mouseData.y > kbm2c_config.mouse_decay) kbm2c_mouseData.y -= kbm2c_config.mouse_decay;
			else if (kbm2c_mouseData.y < -kbm2c_config.mouse_decay) kbm2c_mouseData.y += kbm2c_config.mouse_decay;
		}
	}
	else {
		return;
	}

	// at this point, we should send a report

	memset(&ctrler_data, 0, sizeof(ctrler_data_t));

	if (KBM2C_CHECKKEY(KEYCODE_W, kbm2c_keyFlags)) {
		ctrler_data.left_y -= 127;
	}

	if (KBM2C_CHECKKEY(KEYCODE_S, kbm2c_keyFlags)) {
		ctrler_data.left_y += 127;
	}

	if (KBM2C_CHECKKEY(KEYCODE_A, kbm2c_keyFlags)) {
		ctrler_data.left_x -= 127;
	}

	if (KBM2C_CHECKKEY(KEYCODE_D, kbm2c_keyFlags)) {
		ctrler_data.left_x += 127;
	}

	if (KBM2C_CHECKMODKEY(KEYCODE_MOD_LEFT_ALT, kbm2c_keyFlags)) {
		kbm2c_rightGainIdx = 1;
		led_3_on();
	}
	else {
		kbm2c_rightGainIdx = 0;
		led_3_off();
	}

	ctrler_data.right_x = kbm2c_mouseData.x;
	ctrler_data.right_y = kbm2c_mouseData.y;

	if ((kbm2c_mouseData.btn & (1 << 0)) != 0) {
		ctrler_data.btnBits |= (1 << CTRLBTN_BUMPER_RIGHT);
	}

	if ((kbm2c_mouseData.btn & (1 << 1)) != 0) {
		ctrler_data.btnBits |= (1 << CTRLBTN_BUMPER_LEFT);
	}

	if ((kbm2c_mouseData.btn & (1 << 2)) != 0) {
		ctrler_data.btnBits |= (1 << CTRLBTN_R3);
	}

	if (kbm2c_mouseData.w > 3) {
		ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_UP);
	}

	if (kbm2c_mouseData.w < -3) {
		ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_DOWN);
	}

	if (kbm2c_mouseData.w > 0) {
		kbm2c_mouseData.w--;
	}
	else if (kbm2c_mouseData.w < 0) {
		kbm2c_mouseData.w++;
	}

	if (KBM2C_CHECKKEY(KEYCODE_R, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_SQR);
	}

	if (KBM2C_CHECKKEY(KEYCODE_C, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_CIR);
	}

	if (KBM2C_CHECKKEY(KEYCODE_Q, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_SELECT);
	}

	if (KBM2C_CHECKKEY(KEYCODE_G, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_TRIGGER_LEFT);
	}

	if (KBM2C_CHECKKEY(KEYCODE_F, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_TRIGGER_RIGHT);
	}

	if (KBM2C_CHECKKEY(KEYCODE_SPACE, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_X);
	}

	if (KBM2C_CHECKKEY(KEYCODE_1, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_TRI);
	}

	if (KBM2C_CHECKMODKEY(KEYCODE_MOD_LEFT_SHIFT, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_L3);
	}

	if (KBM2C_CHECKKEY(KEYCODE_2, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_LEFT);
	}

	if (KBM2C_CHECKKEY(KEYCODE_3, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_RIGHT);
	}

	if (KBM2C_CHECKKEY(KEYCODE_4, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_UP);
	}

	if (KBM2C_CHECKKEY(KEYCODE_ESC, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_START);
	}

	if (KBM2C_CHECKKEY(KEYCODE_HOME, kbm2c_keyFlags)) {
		ctrler_data.btnBits |= (1 << CTRLBTN_HOME);
	}

	if (USBH_DS4_NewData != 0)
	{
		// TODO decode DS4 data here
		USBH_DS4_NewData = 0;
	}

	kbm2c_prepForDS3();
	USBD_Dev_DS3_SendReport(&USB_OTG_Core_dev, (uint8_t*)&ds3_packet, sizeof(ds3_packet_t));

	memcpy(kbm2c_keyFlagsPrev, kbm2c_keyFlags, KBM2C_KEYFLAGS_SIZE * sizeof(uint32_t));
}

void kbm2c_prepForDS3()
{
	if (USBH_DS3_Available != 0) {
		memcpy(&ds3_packet, USBH_DS_Buffer, sizeof(ds3_packet_t));
	}
	else {
		memset(&ds3_packet, 0, sizeof(ds3_packet_t));
		ds3_packet.btnBits = 0;
	}

	if (USBH_DS4_Available != 0)
	{
		uint8_t btnBits = USBH_DS_Buffer[5];
		if ((btnBits & (1 << 4)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_SQR);
		if ((btnBits & (1 << 5)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_X);
		if ((btnBits & (1 << 6)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_CIR);
		if ((btnBits & (1 << 7)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_TRI);
		btnBits = USBH_DS_Buffer[6];
		if ((btnBits & (1 << 0)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_BUMPER_LEFT);
		if ((btnBits & (1 << 1)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_BUMPER_RIGHT);
		if ((btnBits & (1 << 2)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_TRIGGER_LEFT);
		if ((btnBits & (1 << 3)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_TRIGGER_RIGHT);
		if ((btnBits & (1 << 4)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_SELECT);
		if ((btnBits & (1 << 5)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_START);
		if ((btnBits & (1 << 6)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_L3);
		if ((btnBits & (1 << 7)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_R3);
		btnBits = USBH_DS_Buffer[7];
		if ((btnBits & (1 << 0)) != 0) ctrler_data.btnBits |= (1 << CTRLBTN_HOME);

		uint8_t dpad = USBH_DS_Buffer[5] & 0x0F;
		if (dpad == 0) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_UP);
		}
		else if (dpad == 1) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_UP) | (1 << CTRLBTN_DPAD_RIGHT);
		}
		else if (dpad == 2) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_RIGHT);
		}
		else if (dpad == 3) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_DOWN) | (1 << CTRLBTN_DPAD_RIGHT);
		}
		else if (dpad == 4) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_DOWN);
		}
		else if (dpad == 5) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_DOWN) | (1 << CTRLBTN_DPAD_LEFT);
		}
		else if (dpad == 6) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_LEFT);
		}
		else if (dpad == 7) {
			ctrler_data.btnBits |= (1 << CTRLBTN_DPAD_UP) | (1 << CTRLBTN_DPAD_LEFT);
		}
	}

	ds3_packet.report_id = 0x01;
	ds3_packet.reserved = 0x00;

	int i, j;
	for (i = 0; i <= (int)CTRLBTN_HOME; i++)
	{
		ds3_packet.btnPressure[i] = 0;
		if ((ctrler_data.btnBits & (1UL << i)) != 0)
		{
			ds3_packet.btnBits |= 1UL << i;
			if (i >= (int)CTRLBTN_DPAD_UP && i <= (int)CTRLBTN_SQR && ds3_packet.btnPressure[i] == 0) {
				// only these buttons have pressures
				// and the pressure is not really 0xFF for light taps
				// so we randomly generate
				ds3_packet.btnPressure[i] = 32 + (rand() % (256 - 32));
			}
		}
	}

	if (USBH_DS3_Available == 0)
	{
		for (j = 27, i = 0; j < 40; j++, i++) {
			ds3_packet.unused[i] = 0;
		}
	}

	int16_t temp;

	kbm2c_coordCalc(&(ctrler_data.left_x), &(ctrler_data.left_y), kbm2c_config.left_stick_gain[kbm2c_leftGainIdx], kbm2c_config.left_stick_deadzone, kbm2c_config.left_stick_max, kbm2c_config.left_stick_curve);
	kbm2c_coordCalc(&(ctrler_data.right_x), &(ctrler_data.right_y), kbm2c_config.right_stick_gain[kbm2c_rightGainIdx], kbm2c_config.right_stick_deadzone, kbm2c_config.right_stick_max, kbm2c_config.right_stick_curve);

	int16_t dsFillerX, dsFillerY;

	dsFillerX = (USBH_DS4_Available != 0) ? (USBH_DS_Buffer[1]) : ((USBH_DS3_Available != 0) ? USBH_DS_Buffer[3] : 0);
	dsFillerY = (USBH_DS4_Available != 0) ? (USBH_DS_Buffer[2]) : ((USBH_DS3_Available != 0) ? USBH_DS_Buffer[4] : 0);
	kbm2c_inactiveStickCalc(&(ctrler_data.left_x), &(ctrler_data.left_y), &(kbm2c_randomDeadZone[0]), kbm2c_config.left_stick_deadzone, dsFillerX, dsFillerY);

	dsFillerX = (USBH_DS4_Available != 0) ? (USBH_DS_Buffer[3]) : ((USBH_DS3_Available != 0) ? USBH_DS_Buffer[5] : 0);
	dsFillerY = (USBH_DS4_Available != 0) ? (USBH_DS_Buffer[4]) : ((USBH_DS3_Available != 0) ? USBH_DS_Buffer[6] : 0);
	kbm2c_inactiveStickCalc(&(ctrler_data.right_x), &(ctrler_data.right_y), &(kbm2c_randomDeadZone[2]), kbm2c_config.right_stick_deadzone, dsFillerX, dsFillerY);


	temp = lround(ctrler_data.left_x);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	ds3_packet.stick_left_x = temp;
	temp = lround(ctrler_data.left_y);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	ds3_packet.stick_left_y = temp;

	temp = lround(ctrler_data.right_x);
	temp += 127;
	temp += kbm2c_testOffset;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	ds3_packet.stick_right_x = temp;
	temp = lround(ctrler_data.right_y);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	ds3_packet.stick_right_y = temp;
}

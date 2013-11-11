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

#define KBM2C_MOUSE_FILTER 0.5d
#define KBM2C_MOUSE_DECAY 20.0d

uint32_t kbm2c_keyFlags[KBM2C_KEYFLAGS_SIZE];
uint32_t kbm2c_keyFlagsPrev[KBM2C_KEYFLAGS_SIZE];
mouse_data_t kbm2c_mouseData;
int16_t kbm2c_randomDeadZone[4] = { 0, 0, 0, 0, };
volatile uint32_t kbm2c_mouseTimestamp;

int8_t kbm2c_stickLookUp[130] = { 0,1,2,11,16,20,23,25,28,30,32,34,36,38,39,41,43,44,45,47,48,50,51,52,53,54,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,74,75,76,77,78,79,80,80,81,82,83,83,84,85,86,87,87,88,89,89,90,91,92,92,93,94,94,95,96,96,97,98,98,99,100,100,101,102,102,103,103,104,105,105,106,107,107,108,108,109,110,110,111,111,112,112,113,114,114,115,115,116,116,117,118,118,119,119,120,120,121,121,122,122,123,123,124,124,125,125,126,126,127,128,128, };

// special testing utilities
int16_t kbm2c_testOffset = 0;

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

	kbm2c_mouseData.x = (xd * KBM2C_MOUSE_FILTER) + (kbm2c_mouseData.x * (1.0d - KBM2C_MOUSE_FILTER));
	kbm2c_mouseData.y = (yd * KBM2C_MOUSE_FILTER) + (kbm2c_mouseData.y * (1.0d - KBM2C_MOUSE_FILTER));

	kbm2c_mouseData.btn = data[parser->mouse_but_bitidx / 8];
	// TODO: add more than 8 buttons?
}

void kbm2c_deadZoneCalc(ctrler_data_t* ctrler_data, double dz, int leftOrRight)
{
	double x;
	double y;

	if (leftOrRight != 0) {
		x = ctrler_data->right_x;
		y = ctrler_data->right_y;
	}
	else {
		x = ctrler_data->left_x;
		y = ctrler_data->left_y;
	}

	double dzx;
	double dzy;

	if (x != 0 && y != 0)
	{
		double atanRes = atan(fabs(y / x));
		dzx = fabs(dz*cos(atanRes));
		dzy = fabs(dz*sin(atanRes));

		if (x > 0) {
			x += dzx;
		}
		else {
			x -= dzx;
		}
		if (y > 0) {
			y += dzy;
		}
		else {
			y -= dzy;
		}
	}
	else
	{
		static uint8_t randDzCnt;

		if (x > 0)
		{
			x += dz;
		}
		else if (x < 0)
		{
			x -= dz;
		}
		else
		{
			int idx = (leftOrRight * 2) + 0;
			if (randDzCnt > (50 + (rand() % 50)) || kbm2c_randomDeadZone[idx] == 0) {
				randDzCnt = 0;
				kbm2c_randomDeadZone[idx] = (-lround(dz) / 2) + (rand() % lround(dz));
			}
			randDzCnt++;
			x = kbm2c_randomDeadZone[idx];
		}

		if (y > 0)
		{
			y += dz;
		}
		else if (y < 0)
		{
			y -= dz;
		}
		else
		{
			int idx = (leftOrRight * 2) + 1;
			if (randDzCnt > (50 + (rand() % 50)) || kbm2c_randomDeadZone[idx] == 0) {
				randDzCnt = 0;
				kbm2c_randomDeadZone[idx] = (-lround(dz) / 2) + (rand() % lround(dz));
			}
			randDzCnt++;
			y = kbm2c_randomDeadZone[idx];
		}
	}

	if (leftOrRight != 0) {
		ctrler_data->right_x = x;
		ctrler_data->right_y = y;
	}
	else {
		ctrler_data->left_x = x;
		ctrler_data->left_y = y;
	}
}

void kbm2c_report(USB_OTG_CORE_HANDLE *pcore)
{
	static ctrler_data_t ctrler_data;
	static ds3_packet_t ds3_packet;

	memset(&ctrler_data, 0, sizeof(ctrler_data_t));
	memset(&ds3_packet, 0, sizeof(ds3_packet_t));

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

	uint32_t curT = systick_1ms_cnt;
	if ((curT - kbm2c_mouseTimestamp) > 21)
	{
		kbm2c_mouseTimestamp = systick_1ms_cnt;
		kbm2c_mouseData.x = 0;
		kbm2c_mouseData.y = 0;
	}
	else if ((curT - kbm2c_mouseTimestamp) > 14)
	{
		kbm2c_mouseTimestamp = systick_1ms_cnt;
		kbm2c_mouseData.x = (kbm2c_mouseData.x * (1.0d - KBM2C_MOUSE_FILTER));
		kbm2c_mouseData.y = (kbm2c_mouseData.y * (1.0d - KBM2C_MOUSE_FILTER));
		if (kbm2c_mouseData.x > KBM2C_MOUSE_DECAY) kbm2c_mouseData.x -= KBM2C_MOUSE_DECAY;
		else if (kbm2c_mouseData.x < -KBM2C_MOUSE_DECAY) kbm2c_mouseData.x += KBM2C_MOUSE_DECAY;
		if (kbm2c_mouseData.y > KBM2C_MOUSE_DECAY) kbm2c_mouseData.y -= KBM2C_MOUSE_DECAY;
		else if (kbm2c_mouseData.y < -KBM2C_MOUSE_DECAY) kbm2c_mouseData.y += KBM2C_MOUSE_DECAY;
	}

	int i;
	i = lround(kbm2c_mouseData.x);
	if (KBM2C_CHECKMODKEY(KEYCODE_MOD_LEFT_ALT, kbm2c_keyFlags)) {
		i *= 32;
	}
	if (i < 0) i = -i;
	if (i > 127) i = 127;
	ctrler_data.right_x = kbm2c_stickLookUp[i];
	if (kbm2c_mouseData.x < 0) ctrler_data.right_x = -ctrler_data.right_x;
	i = lround(kbm2c_mouseData.y);
	if (KBM2C_CHECKMODKEY(KEYCODE_MOD_LEFT_ALT, kbm2c_keyFlags)) {
		i *= 32;
		led_3_on();
	}
	else {
		led_3_off();
	}
	if (i < 0) i = -i;
	if (i > 127) i = 127;
	ctrler_data.right_y = kbm2c_stickLookUp[i];
	if (kbm2c_mouseData.y < 0) ctrler_data.right_y = -ctrler_data.right_y;

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

	kbm2c_prepForDS3(&ctrler_data, &ds3_packet);
	USBD_Dev_DS3_SendReport(pcore, (uint8_t*)&ds3_packet, sizeof(ds3_packet_t));

	memcpy(kbm2c_keyFlagsPrev, kbm2c_keyFlags, KBM2C_KEYFLAGS_SIZE * sizeof(uint32_t));
}

void kbm2c_prepForDS3(ctrler_data_t* ctrler_data, ds3_packet_t* packet)
{
	packet->report_id = 0x01;
	packet->reserved = 0x00;

	packet->btnBits = 0;
	int i, j;
	for (i = 0; i <= (int)CTRLBTN_HOME; i++)
	{
		packet->btnPressure[i] = 0;
		if ((ctrler_data->btnBits & (1UL << i)) != 0)
		{
			packet->btnBits |= 1UL << i;
			if (i >= (int)CTRLBTN_DPAD_UP && i <= (int)CTRLBTN_SQR) {
				// only these buttons have pressures
				// and the pressure is not really 0xFF for light taps
				// so we randomly generate
				packet->btnPressure[i] = 32 + (rand() % (256 - 32));
			}
		}
	}

	for (j = 27, i = 0; j < 40; j++, i++) {
		packet->unused[i] = 0;
	}

	int16_t temp;

	kbm2c_deadZoneCalc(ctrler_data, 28.0d, 0);
	kbm2c_deadZoneCalc(ctrler_data, 28.0d, 1);

	temp = lround(ctrler_data->left_x);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	packet->stick_left_x = temp;
	temp = lround(ctrler_data->left_y);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	packet->stick_left_y = temp;
	temp = lround(ctrler_data->right_x);
	temp += 127;
	temp += kbm2c_testOffset;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	packet->stick_right_x = temp;
	temp = lround(ctrler_data->right_y);
	temp += 127;
	temp = KBM2C_CONSTRAIN(temp, 0, 255);
	packet->stick_right_y = temp;
}

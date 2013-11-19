#ifndef KBM2CTRL_H_
#define KBM2CTRL_H_

#include <stdint.h>
#include <usbotg_lib/usb_core.h>
#include "hidrpt.h"

// DEFINITIONS

#define KBM2C_MAX_KEYACTIONS   32
#define KBM2C_MAX_MOUSEACTIONS 16
#define KBM2C_KEYFLAGS_SIZE    (256 / 32)

#define KBM2C_CONSTRAIN(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))
#define KBM2C_CHECKKEY(x, flags) (((flags)[(x) / 32] & (1UL << ((x) % 32))) != 0)
#define KBM2C_CHECKMODKEY(x, flags) (((flags)[KBM2C_KEYFLAGS_SIZE - 1] & (x)) != 0)
#define KBM2C_APPLY_DEADZONE(x, dz) (((x) > 0) ? ((x) + (dz)) : (((x) < 0) ? ((x) - (dz)) : (x)))

// TYPEDEFS

typedef enum
{
	KBM2C_OK = 0,
	KBM2C_ERR_CHKSUM,
	KBM2C_ERR_FLASH_NOT_EMPTY,
}
kbm2c_err_t;

typedef enum
{
	MOUSE_TO_LEFT_STICK,
	MOUSE_TO_LEFT_STICK_INV,
	MOUSE_TO_RIGHT_STICK,
	MOUSE_TO_RIGHT_STICK_INV,
}
mousemap_t;

typedef struct
{
	uint8_t modifier;
	uint8_t key;
}
keyboard_data_pair_t;

typedef struct
{
	uint8_t      key;
	//ctrlaction_t action;
}
keyaction_pair_t;

typedef struct
{
	uint8_t      button;
	//ctrlaction_t action;
}
mouseaction_pair_t;

typedef struct
{
	int16_t val;
	char shouldPress;
	char shouldRelease;
	enum {
		TRIGSTATE_RELEASED,
		TRIGSTATE_PRESSING,
		TRIGSTATE_PRESSED,
		TRIGSTATE_RELEASING,
	} state;
}
kbm2c_trigger_data_t;

typedef struct
{
	uint8_t on_min;
	uint8_t on_max;
	uint8_t off_min;
	uint8_t off_max;
}
turbo_config_t;

typedef enum
{
	NO_CONFIG = 0x00,
	CONFIG_PLAYSTATION = 0x01,
	CONFIG_XBOX = 0x02,
}
config_flags_t;

typedef struct
{
	uint8_t             flags; // 0 for not used, checked to see if this config actually exists
	mousemap_t          mousemap;
	keyaction_pair_t    keyaction[KBM2C_MAX_KEYACTIONS];
	mouseaction_pair_t  mouseaction[KBM2C_MAX_MOUSEACTIONS];
	uint8_t             left_stick_deadzone;
	uint8_t             right_stick_deadzone;
	uint16_t            left_stick_max;
	uint16_t            right_stick_max;
	uint8_t             trigger_up_speed;
	uint8_t             trigger_down_speed;
	double              left_stick_gain[3];
	double              right_stick_gain[3];
	uint8_t             left_stick_curve[131];
	uint8_t             right_stick_curve[131];
	double              mouse_filter;
	double              mouse_decay;

	float               wasd_accel;
	float               wasd_decay;
	turbo_config_t      turbo_config;
	uint16_t            checksum;
}
kbm2c_config_t;

typedef struct
{
	double  x;
	double  y;
	int16_t  w;
	uint16_t btn;
}
mouse_data_t;

typedef union
{
	uint16_t u16;
	keyboard_data_pair_t d;
}
keyboard_data_t;

typedef enum
{
	// these use dualshock3's bit ordering
	CTRLBTN_SELECT,
	CTRLBTN_L3,
	CTRLBTN_R3,
	CTRLBTN_START,
	CTRLBTN_DPAD_UP,
	CTRLBTN_DPAD_RIGHT,
	CTRLBTN_DPAD_DOWN,
	CTRLBTN_DPAD_LEFT,
	CTRLBTN_TRIGGER_LEFT,
	CTRLBTN_TRIGGER_RIGHT,
	CTRLBTN_BUMPER_LEFT,
	CTRLBTN_BUMPER_RIGHT,
	CTRLBTN_TRI,
	CTRLBTN_CIR,
	CTRLBTN_X,
	CTRLBTN_SQR,
	CTRLBTN_HOME,

	// these are out of order
	CTRLBTN_TRIGGER_LEFT_HALF,
	CTRLBTN_TRIGGER_RIGHT_HALF,
	CTRL_STICK_UP,
	CTRL_STICK_DOWN,
	CTRL_STICK_LEFT,
	CTRL_STICK_RIGHT,
	CTRL_LEFTSTICK_GAIN_ALT,
	CTRL_RIGHTSTICK_GAIN_ALT,
	CTRL_REMAP_MOUSE_TO_ACCEL,
	CTRL_REMAP_MOUSE_TO_ACCEL_INV,
	CTRL_REMAP_MOUSE_TO_GYRO,
	CTRL_REMAP_MOUSE_TO_GYRO_INV,
	CTRL_FLAG_TURBO = 0x80,
}
ctrlbtn_t;

typedef enum
{
	DS4BTN_DPAD0,
	DS4BTN_DPAD1,
	DS4BTN_DPAD2,
	DS4BTN_DPAD3,
	DS4BTN_SQR,
	DS4BTN_X,
	DS4BTN_CIR,
	DS4BTN_TRI,
	DS4BTN_L1,
	DS4BTN_R1,
	DS4BTN_L2,
	DS4BTN_R2,
	DS4BTN_SHARE,
	DS4BTN_OPT,
	DS4BTN_L3,
	DS4BTN_R3,
	DS4BTN_PS,
	DS4BTN_TPAD,
}
ds4btn_t;

typedef struct
{
	double left_x;
	double left_y;
	double right_x;
	double right_y;
	int16_t left_trigger;
	int16_t right_trigger;
	uint32_t btnBits;
}
ctrler_data_t;

typedef struct
{
	uint8_t report_id;
	uint8_t reserved;
	uint32_t btnBits;
	uint8_t stick_left_x;
	uint8_t stick_left_y;
	uint8_t stick_right_x;
	uint8_t stick_right_y;
	uint8_t btnPressure[8 + 8 + 1]; // ends at 26
	uint8_t unused[13]; // ends at 39
	uint16_t accel_x;
	uint16_t accel_y;
	uint16_t accel_z;
	uint16_t gyro;
}
ds3_packet_t;

typedef struct
{
	uint8_t report_id;
	uint8_t stick_left_x;
	uint8_t stick_left_y;
	uint8_t stick_right_x;
	uint8_t stick_right_y;
	uint16_t btnBits;
	uint8_t counter;
	uint8_t left_trigger;
	uint8_t right_trigger;
	uint8_t unknown1;
	uint8_t unknown2;
	uint8_t unknown3;
	int16_t gyro_x;
	int16_t gyro_y;
	int16_t gyro_z;
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	uint8_t unknown4; // 0x08
	uint8_t unknown5; // 0x00
	uint8_t unknown6; // 0x00
	uint8_t unknown7; // 0x00
	uint8_t unknown8; // 0x00
	uint8_t unknown9; // 0x00
	uint8_t unknown10; // 0x1B
	uint8_t unknown11; // 0x00
	uint8_t unknown12; // 0x00
	uint8_t unknown13; // 0x01, sometimes 0x02, possibly number of touches
	// TODO: more reverse engineering
	uint8_t the_rest[64 - 25]; // starting from idx 25
}
ds4_packet_t;

// EXTERN VARIABLES

extern kbm2c_config_t kbm2c_config;

// FUNCTION PROTOTYPES

void kbm2c_init();
void kbm2c_handleMouseReport(uint8_t* data, uint8_t len, HID_Rpt_Parsing_Params_t* parser);
void kbm2c_handleKeyReport(uint8_t modifier, uint8_t* data, uint8_t len);
void kbm2c_handleDs4Report(uint8_t* data);
void kbm2c_task(char force);
void kbm2c_prepForDS3();
void kbm2c_prepForDS4();
void kbm2c_coordCalc(double* xptr, double* yptr, double gain, double dz, double lim, uint8_t* curve);
void kbm2c_deadZoneCalc(double* xptr, double* yptr, double dz);
void kbm2c_inactiveStickCalc(double* xptr, double* yptr, int8_t* randDeadZone, uint8_t dz, int16_t fillerX, int16_t fillerY);
kbm2c_err_t kbm2c_config_default(kbm2c_config_t* dest);

#endif

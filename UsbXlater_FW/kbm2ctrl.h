#ifndef KBM2CTRL_H_
#define KBM2CTRL_H_

#include <stdint.h>
#include <usbotg_lib/usb_core.h>

#define KBM2C_MAX_KEYACTIONS   32
#define KBM2C_MAX_MOUSEACTIONS 16
#define KBM2C_KEYFLAGS_SIZE    (256 / 32)

#define KBM2C_CONSTRAIN(x, min, max) (((x) > (max)) ? (max) : (((x) < (min)) ? (min) : (x)))
#define KBM2C_CHECKKEY(x, flags) (((flags)[(x) / 32] & (1UL << ((x) % 32))) != 0)
#define KBM2C_CHECKMODKEY(x, flags) (((flags)[KBM2C_KEYFLAGS_SIZE - 1] & (x)) != 0)
#define KBM2C_APPLY_DEADZONE(x, dz) (((x) > 0) ? ((x) + (dz)) : (((x) < 0) ? ((x) - (dz)) : (x)))

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

typedef enum
{
	TRIGSTATE_RELEASED,
	TRIGSTATE_GOING_DOWN,
	TRIGSTATE_FULLY_DOWN,
	TRIGSTATE_GOING_UP,
}
trigger_state_t;

typedef enum
{
	TRIGQUEUE_NONE = 0x00,
	TRIGQUEUE_DOWN = 0x01,
	TRIGQUEUE_UP   = 0x02,
}
trigger_queue_t;

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
	uint8_t             left_deadzone;
	uint8_t             right_deadzone;
	uint16_t            stick_max;
	uint8_t             trigger_speed;
	float               left_gain;
	float               right_gain;
	float               left_gain_alt;
	float               right_gain_alt;
	float               left_curve;
	float               right_curve;
	float               wasd_accel;
	float               wasd_decay;
	turbo_config_t      turbo_config;
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

extern volatile uint8_t kbm2c_mouseTimeSince;

void kbm2c_mouseDidNotMove();
void kbm2c_handleMouseReport(uint8_t* data, uint8_t len);
void kbm2c_handleKeyReport(uint8_t modifier, uint8_t* data, uint8_t len);
void kbm2c_report(USB_OTG_CORE_HANDLE *pcore);

#endif

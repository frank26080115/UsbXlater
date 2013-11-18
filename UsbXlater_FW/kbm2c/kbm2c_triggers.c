#include "kbm2ctrl.h"

#define KBM2C_TRIG_MAX 0xFF
#define KBM2C_TRIG_RAND 16

kbm2c_trigger_data_t kbm2c_triggerLeft;
kbm2c_trigger_data_t kbm2c_triggerRight;

char kbm2c_trigger_proc(kbm2c_trigger_data_t* trig_data, char isPressed)
{
	if (isPressed != 0) {
		trig_data->shouldPress = 1;
	}
	else {
		trig_data->shouldRelease = 1;
	}

	if ((trig_data->state == TRIGSTATE_RELEASED && trig_data->shouldPress != 0) || trig_data->state == TRIGSTATE_PRESSING)
	{
		trig_data->shouldPress = 0;
		trig_data->state = TRIGSTATE_PRESSING;
		trig_data->val += kbm2c_config.trigger_down_speed - KBM2C_TRIG_RAND + (rand() % (KBM2C_TRIG_RAND * 2));
		if (trig_data->val >= KBM2C_TRIG_MAX) {
			trig_data->val = KBM2C_TRIG_MAX;
			trig_data->state = TRIGSTATE_PRESSED;
		}
	}
	else if ((trig_data->state == TRIGSTATE_PRESSED && trig_data->shouldRelease != 0) || trig_data->state == TRIGSTATE_RELEASING)
	{
		trig_data->val -= kbm2c_config.trigger_up_speed - KBM2C_TRIG_RAND + (rand() % (KBM2C_TRIG_RAND * 2));
		if (trig_data->val <= 0)
		{
			trig_data->shouldRelease = 0;
			trig_data->val = 0;
			trig_data->state = TRIGSTATE_RELEASED;
		}
	}

	return trig_data->val != 0;
}

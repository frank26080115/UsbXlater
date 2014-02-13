#ifndef USBH_DEV_DUALSHOCK_H_
#define USBH_DEV_DUALSHOCK_H_

#include <stdint.h>
#include <usbh_lib/usbh_core.h>

#define SONY_VID 0x054C
#define DUALSHOCK3_PID 0x0268
#define DUALSHOCK4_PID 0x05C4

typedef enum {
	USBH_DSPT_TASK_NONE,
	USBH_DSPT_TASK_FEATURE_14_1401,
	USBH_DSPT_TASK_FEATURE_14_1402,
}
USBH_DS_task_t;

extern USBH_DS_task_t USBH_DS_PostedTask;

extern char USBH_DS3_Available;
extern char USBH_DS4_Available;
extern char USBH_DS3_NewData;
extern char USBH_DS4_NewData;
extern uint8_t USBH_DS_Buffer[64];

void USBH_DS4_Init_Handler(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev);
void USBH_DS4_Task(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev);
void USBH_DS4_DeInit_Handler(USB_OTG_CORE_HANDLE* pcore, USBH_DEV* pdev);
void USBH_DS4_Data_Handler(void* p_io, uint8_t* data, uint16_t len);
void USBH_DS3_Data_Handler(void* p_io, uint8_t* data, uint16_t len);

#endif

#ifndef DS4_EMU_H_
#define DS4_EMU_H_

#include <stdint.h>
#include <btstack/btproxy.h>
#include <btstack/src/l2cap.h>
#include <btstack/src/hci.h>

#define DS4_REPORT_A3_LEN (8*6)
#define DS4_REPORT_02_LEN 37

#define DS4_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE_SIZE 697
#define PS4_SDP_SERVICE_SEARCH_ATTRIBUTE_RESPONSE_SIZE 333

typedef enum {
	EMUSTATE_NONE = 0,
	EMUSTATE_HAS_DS4_BDADDR = 0x01,
	EMUSTATE_HAS_PS4_BDADDR = 0x02,
	EMUSTATE_HAS_BT_BDADDR = 0x04,
	EMUSTATE_GIVEN_PS4_BDADDR = 0x08,
	EMUSTATE_HAS_MFG_DATE = 0x10,
	EMUSTATE_IS_DS4_USB_CONNECTED = 0x20,
	EMUSTATE_IS_DS4_BT_CONNECTED = 0x40,
	EMUSTATE_IS_PS4_BT_CONNECTED = 0x80,
	EMUSTATE_HAS_REPORT_02 = 0x100,
	EMUSTATE_GIVEN_DS4_1401 = 0x200,
	EMUSTATE_GIVEN_DS4_1402 = 0x400,
}
EMU_State_t;

typedef enum
{
	EMUUSBDMODE_NONE,
	EMUUSBDMODE_DS4,
	EMUUSBDMODE_DS3,
	EMUUSBDMODE_KBM,
	EMUUSBDMODE_VCP,
	EMUUSBDMODE_BOOTLOADER,
}
EMU_USBD_Mode_t;

typedef enum
{
	USBDHOST_NONE,
	USBDHOST_PS4,
	USBDHOST_PS3,
	USBDHOST_UNKNOWN,
}
USBD_Host_t;

extern const uint8_t ps4_sdp_service_search_attribute_response[];
extern const uint8_t ds4_extended_inquiry_response[];
extern const uint8_t ds4_sdp_service_search_attribute_response[];
extern const uint8_t ds4_sdp_service_search_pattern[];
extern const uint8_t ds4_sdp_attribute_id_list[];
extern const uint8_t ps4_sdp_service_search_pattern[];
extern const uint8_t ps4_sdp_attribute_id_list[];
extern const uint8_t ds4_link_key[];
extern const uint8_t ds4_bt_device_name[];

extern volatile EMU_State_t DS4EMU_State;
extern volatile USBD_Host_t EMU_USBD_Host;
extern uint8_t ds4_bdaddr[BD_ADDR_LEN];
extern uint8_t ps4_bdaddr[BD_ADDR_LEN];
extern uint8_t ds4_reportA3[DS4_REPORT_A3_LEN];
extern uint8_t ds4_report02[DS4_REPORT_02_LEN];
extern uint8_t ps4_link_key[LINK_KEY_LEN];

#endif

#ifndef USBD_DEV_INC_ALL_H_
#define USBD_DEV_INC_ALL_H_

#include "usbd_dev_ds3.h"
#include "usbd_dev_cdc.h"

#define USBD_LANGIDDESC_SIZE 4
#define USBD_HIDDESCRIPTOR_SIZE 9
#define HID_DESCRIPTOR_TYPE           0x21
#define HID_REPORT_DESC               0x22
#define HID_REQ_SET_PROTOCOL          0x0B
#define HID_REQ_GET_PROTOCOL          0x03
#define HID_REQ_SET_IDLE              0x0A
#define HID_REQ_GET_IDLE              0x02
#define HID_REQ_SET_REPORT            0x09
#define HID_REQ_GET_REPORT            0x01

#endif /* USBD_DEV_INC_ALL_H_ */

#ifndef USBD_DEV_CDC_H_
#define USBD_DEV_CDC_H_

#include <stdint.h>

#include <usbd_lib/usbd_core.h>
#include <usbd_lib/usbd_req.h>
#include <usbd_lib/usbd_conf.h>
#include <usbd_lib/usbd_ioreq.h>
#include <usbd_lib/usb_dcd.h>
#include <usbotg_lib/usb_regs.h>

#include <ringbuffer.h>

#define CDC_DATA_MAX_PACKET_SIZE       64   /* Endpoint IN & OUT Packet size */
#define CDC_CMD_PACKET_SZE             8    /* Control Endpoint Packet size */
#define CDC_IN_FRAME_INTERVAL          5    /* Number of frames between IN transfers */

#define USBD_Dev_CDC_D2H_EP 0x81
#define USBD_Dev_CDC_H2D_EP 0x02
#define USBD_Dev_CDC_CMD_EP 0x82

#define CDC_DATA_IN_PACKET_SIZE                CDC_DATA_MAX_PACKET_SIZE
#define CDC_DATA_OUT_PACKET_SIZE               CDC_DATA_MAX_PACKET_SIZE
#define USBD_Dev_CDC_D2H_EP_SZ                 CDC_DATA_IN_PACKET_SIZE
#define USBD_Dev_CDC_H2D_EP_SZ                 CDC_DATA_OUT_PACKET_SIZE
#define USBD_Dev_CDC_CMD_EP_SZ                 CDC_CMD_PACKET_SZE
//#define CDC_D2H_FIFO_SIZE                      (256-16)

#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05
#define USB_SIZ_DEVICE_DESC                     18
#define USB_SIZ_STRING_LANGID                   4

#define USB_CDC_CONFIG_DESC_SIZ                (67)
#define USB_CDC_DESC_SIZ                       (67-9)

#define CDC_DESCRIPTOR_TYPE                     0x21

#define DEVICE_CLASS_CDC                        0x02
#define DEVICE_SUBCLASS_CDC                     0x00


#define USB_DEVICE_DESCRIPTOR_TYPE              0x01
#define USB_CONFIGURATION_DESCRIPTOR_TYPE       0x02
#define USB_STRING_DESCRIPTOR_TYPE              0x03
#define USB_INTERFACE_DESCRIPTOR_TYPE           0x04
#define USB_ENDPOINT_DESCRIPTOR_TYPE            0x05

#define STANDARD_ENDPOINT_DESC_SIZE             0x09

extern volatile ringbuffer_t USBD_CDC_H2D_FIFO;
extern volatile ringbuffer_t USBD_CDC_D2H_FIFO;
extern USBD_Device_cb_TypeDef USBD_Dev_CDC_cb;
extern char USBD_CDC_IsReady;

#endif

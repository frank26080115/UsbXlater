#ifndef HCI_TRANSPORTS_BTPROXY_H_
#define HCI_TRANSPORTS_BTPROXY_H_

#include <btstack/src/hci_transport.h>
#include <btstack/hci_transport_stm32usbh.h>

extern hci_transport_t * hci_transport_h4_instance(void);
extern hci_transport_t * hci_transport_h5_instance(void);
extern hci_transport_t * hci_transport_stm32usbh_instance(void);

#endif /* HCI_TRANSPORTS_BTPROXY_H_ */

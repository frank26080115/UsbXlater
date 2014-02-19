#ifndef BTPROXY_H_
#define BTPROXY_H_

#include <btstack/src/hci.h>
#include <btstack/src/bt_control.h>

extern hci_stack_t* hci_stack; // located in hci.c
extern void (*hal_tick_handler)(void);
extern void hal_dummy_tick_handler(void);
extern void btproxy_packet_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
extern uint32_t btproxy_provide_class_of_device();
extern char* btproxy_provide_name_of_device();
extern char btproxy_sub_handler(void * connection, uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
extern char btproxy_check_bdaddr(bd_addr_t addr);
extern uint8_t* bt_extended_inquiry_response;

void btproxy_init_RN42HCI();
void btproxy_init_USB();
void btproxy_init(hci_uart_config_t* ucfg, bt_control_t* btctrlcfg, hci_transport_t* xport);
void btproxy_task();
void btproxy_runloop_execute_once();

void l2cap_run();

extern void btproxy_db_open();
extern void btproxy_db_close();
extern int btproxy_db_get_link_key(bd_addr_t *bd_addr, link_key_t *link_key, link_key_type_t * type);
extern void btproxy_db_put_link_key(bd_addr_t *bd_addr, link_key_t *key, link_key_type_t type);
extern void btproxy_db_delete_link_key(bd_addr_t *bd_addr);
extern int btproxy_db_get_name(bd_addr_t *bd_addr, device_name_t *device_name);
extern void btproxy_db_put_name(bd_addr_t *bd_addr, device_name_t *device_name);
extern void btproxy_db_delete_name(bd_addr_t *bd_addr);
extern uint8_t btproxy_db_persistent_rfcomm_channel(char *servicename);

#endif

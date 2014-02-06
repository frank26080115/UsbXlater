#ifndef BTPROXY_H_
#define BTPROXY_H_

#include <btstack/src/hci.h>
#include <btstack/src/bt_control.h>

extern hci_stack_t* hci_stack; // located in hci.c
extern void (*hal_tick_handler)(void);
extern void hal_dummy_tick_handler(void);

void btproxy_init_RN42HCI();
void btproxy_init_USB();
void btproxy_init(hci_uart_config_t* ucfg, bt_control_t* btctrlcfg, hci_transport_t* xport);
void btproxy_task();
void btproxy_runloop_execute_once();

#endif

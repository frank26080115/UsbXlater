#include "hci.h"
#include "hci_defs.h"
#include <utilities.h>
#include <flashfile.h>
#include <dbg_wdg.h>
#include <usbotg_lib/usb_core.h>
#include <usbh_dev/usbh_dev_bt_hci.h>
#include <usbh_lib/usbh_core.h>
#include <stm32fx/peripherals.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HCI_TX_BUFF_SIZE 64
uint8_t hci_tx_buff[HCI_TX_BUFF_SIZE];

void HCI_HandleEvent(BTHCI_t* bthci, uint8_t* data)
{
	// packet type is not included

	uint8_t evt_code = data[0];
	uint8_t len = data[1];

	char err = 0;

	dbg_printf(DBGMODE_DEBUG, "HCI_HandleEvent EVT 0x%02X\r\n", evt_code);

	if (evt_code == HCI_EVENT_COMMANDCOMPLETE)
	{
		uint8_t numAllowedCmdPackets = data[2];
		uint16_t cmd = data[3] + (data[4] << 8);
		uint8_t status = data[5];
		uint8_t* bd_addr;

		switch (cmd)
		{
			case HCI_CMD_READBDADDR:
				bd_addr = &data[6];
				bthci->dongle_bdaddr;
				memcpy(bthci->dongle_bdaddr, bd_addr, 6);
				flashfile_cacheFlush();
				nvm_file_t* f = (nvm_file_t*)flashfilesystem.cache;
				if (memcmp(f->d.fmt.dongle_bdaddr, bd_addr, 6) != 0) {
					// copy only if different
					memcpy(f->d.fmt.dongle_bdaddr, bd_addr, 6);
					flashfilesystem.cache_dirty = 1;
				}
				flashfile_cacheFlush();
				break;
		}

		if (status == HCI_COMMANDCOMPLETE_SUCCESS && cmd == bthci->last_cmd)
		{
			bthci->substate |= HCISUBSTATE_COMMANDSTATUS;

			switch (bthci->state & (~HCISTATE_WAIT))
			{
				case HCISTATE_RESET:
					bthci->state = HCISTATE_READBDADDR;
					break;
				case HCISTATE_READBDADDR:
					bthci->state = HCISTATE_WRITECLASSOFDEV;
					break;
				case HCISTATE_WRITECLASSOFDEV:
					bthci->state = HCISTATE_CHANGELOCALNAME;
					break;
				case HCISTATE_CHANGELOCALNAME:
					bthci->state = HCISTATE_WRITEEXTENDEDINQUIRYRESPONSE;
					break;
				case HCISTATE_WRITEEXTENDEDINQUIRYRESPONSE:
					bthci->state = HCISTATE_DELETESTOREDLINKKEY;
					break;
				case HCISTATE_DELETESTOREDLINKKEY:
					bthci->state = HCISTATE_SETEVENTFILTER;
					break;
				case HCISTATE_SETEVENTFILTER:
					bthci->state = HCISTATE_SETEVENTMASK;
					break;
				case HCISTATE_SETEVENTMASK:
					bthci->state = HCISTATE_WRITEINQUIRYMODE;
					break;
				case HCISTATE_WRITEINQUIRYMODE:
					bthci->state = HCISTATE_WRITEDEFAULTLINKPOLICYSETTINGS;
					break;
				case HCISTATE_WRITEDEFAULTLINKPOLICYSETTINGS:
					bthci->state = HCISTATE_WRITEPAGETIMEOUT;
					break;
				case HCISTATE_WRITEPAGETIMEOUT:
					bthci->state = HCISTATE_WRITECONNECTIONACCEPTTIMEOUT;
					break;
				case HCISTATE_WRITECONNECTIONACCEPTTIMEOUT:
					bthci->state = HCISTATE_WRITESIMPLEPAIRINGMODE;
					break;
				case HCISTATE_WRITESIMPLEPAIRINGMODE:
					bthci->state = HCISTATE_WRITEINQUIRYSCANACTIVITY;
					break;
				case HCISTATE_WRITEINQUIRYSCANACTIVITY:
					bthci->state = HCISTATE_WRITEINQUIRYSCANTYPE;
					break;
				case HCISTATE_WRITEINQUIRYSCANTYPE:
					bthci->state = HCISTATE_WRITEPAGESCANACTIVITY;
					break;
				case HCISTATE_WRITEPAGESCANACTIVITY:
					bthci->state = HCISTATE_WRITEPAGESCANTYPE;
					break;
				case HCISTATE_WRITEPAGESCANTYPE:
					bthci->state = HCISTATE_WRITESCANENABLE;
					break;
				case HCISTATE_WRITESCANENABLE:
					bthci->state = HCISTATE_WRITEDEFAULTERRONEOUSDATAREPORTING;
					break;
				case HCISTATE_WRITEDEFAULTERRONEOUSDATAREPORTING:
					bthci->state = HCISTATE_WAITFORUSERTOINITIATECONNECTION;
					dbg_printf(DBGMODE_DEBUG, "HCI waiting for user to initiate connection\r\n");
					break;

				case HCISTATE_AUTHENTICATIONREQUESTED:
					if (cmd == HCI_CMD_LINKKEYREQUESTREPLY) {
						bthci->substate |= HCISUBSTATE_AUTHENTICATIONREQUESTED_LINKKEYREQUESTREPLY_COMMANDCOMPLETE;
					}
					// state unchanged
					break;
				case HCISTATE_CONNECTIONREQUEST_HIDCONTROL:
				case HCISTATE_CONNECTIONREQUEST_HIDINTERRUPT:
					if (cmd == HCI_CMD_LINKKEYREQUESTREPLY) {
						bthci->substate |= HCISUBSTATE_CONNECTIONREQUEST_HID_LINKKEYREQUESTREPLY_COMMANDCOMPLETE;
					}
					// state unchanged
					break;
				case HCISTATE_CONFIGUREREQUEST_HIDCONTROL:
				case HCISTATE_CONFIGUREREQUEST_HIDINTERRUPT:
					if (cmd == HCI_CMD_LINKKEYREQUESTREPLY) {
						bthci->substate |= HCISUBSTATE_CONFIGUREREQUEST_HID_LINKKEYREQUESTREPLY_COMMANDCOMPLETE;
					}
					// state unchanged
					break;
				default:
					err = 1;
					dbg_printf(DBGMODE_ERR, "HCI Command Complete Error, unknown state 0x%02X, ", bthci->state);
					break;
			}
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI Command Complete Error, ");
			if (status != HCI_COMMANDSTATUS_SUCCESS)
			{
				dbg_printf(DBGMODE_ERR, "status = 0x%02X, ");
			}
			if (cmd == bthci->last_cmd)
			{
				dbg_printf(DBGMODE_ERR, "cmd 0x%04X == bthci->last_cmd 0x%04X, ", cmd, bthci->last_cmd);
			}
		}
	}
	else if (evt_code == HCI_EVENT_COMMANDSTATUS)
	{
		uint8_t status = data[2];
		uint8_t numAllowedCmdPackets = data[3];
		uint16_t cmd = data[4] + (data[5] << 8);

		if (status == HCI_COMMANDSTATUS_PENDING && cmd == bthci->last_cmd)
		{
			bthci->substate |= HCISUBSTATE_COMMANDSTATUS;

			switch (bthci->state & (~HCISTATE_WAIT))
			{
				case HCISTATE_CHANGECONNECTIONPACKETTYPE:
					// no state change
					break;
				case HCISTATE_SETCONNECTIONENCRYPTION:
					// no state change
					break;
				default:
					err = 1;
					dbg_printf(DBGMODE_ERR, "HCI Command Status Error, unknown state 0x%02X, ", bthci->state);
					break;
			}
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI Command Status Error, ");
			if (status != HCI_COMMANDSTATUS_PENDING)
			{
				dbg_printf(DBGMODE_ERR, "status = 0x%02X, ");
			}
			if (cmd == bthci->last_cmd)
			{
				dbg_printf(DBGMODE_ERR, "cmd 0x%04X == bthci->last_cmd 0x%04X, ", cmd, bthci->last_cmd);
			}
		}
	}
	else if (evt_code == HCI_EVENT_ROLECHANGE)
	{
		uint8_t status = data[2];
		uint8_t bd_addr[6];
		memcpy(bd_addr, &data[3], 6);
		uint8_t new_role = data[3 + 6];

		if (memcmp(bd_addr, bthci->dongle_bdaddr, 6) == 0)
		{
			bthci->role = new_role;

			if ((bthci->state & (~HCISTATE_WAIT)) == HCISTATE_CREATECONNECTION)
			{
				if (new_role == HCI_ROLE_SLAVE)
				{
					bthci->substate |= HCISUBSTATE_CREATECONNECTION_ROLECHANGE;
					dbg_printf(DBGMODE_DEBUG, "HCI role changed slave\r\n");
				}
				else
				{
					err = 1;
					dbg_printf(DBGMODE_ERR, "HCI event error, wrong role, role change to  0x%02X, ", new_role);
				}
			}
			else
			{
				err = 1;
				dbg_printf(DBGMODE_ERR, "HCI event error, role changed during unknown state 0x%02X, ", bthci->state);
			}
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, role changed for wrong BD_ADDR, ");
			dbg_printf(DBGMODE_ERR, "dongle %s != ", print_bdaddr(bthci->dongle_bdaddr));
			dbg_printf(DBGMODE_ERR, "event %s , ", print_bdaddr(bd_addr));
		}
	}
	else if (evt_code == HCI_EVENT_LINKSUPERVISIONTIMEOUTCHANGED)
	{
		uint16_t conn_handle = data[2] + (data[3] << 8);
		uint16_t slots = data[4] + (data[5] << 8);

		if ((bthci->state & (~HCISTATE_WAIT)) == HCISTATE_AUTHENTICATIONREQUESTED)
		{
			if (bthci->conn_handle == conn_handle)
			{
				bthci->substate |= HCISUBSTATE_AUTHENTICATIONREQUESTED_LINKSUPERVISIONTIMEOUTCHANGED;
			}
			else
			{
				err = 1;
				dbg_printf(DBGMODE_ERR, "HCI event error, link supervision timout changed wrong connection handle, bthci->conn_handle 0x%04X != conn_handle 0x%04X, ", bthci->conn_handle, conn_handle);
			}
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, link supervision timout changed during wrong state, state 0x%02X, ", bthci->state);
		}
	}
	else if (evt_code == HCI_EVENT_AUTHENTICATIONCOMPLETE)
	{
		uint8_t status = data[2];
		uint16_t conn_handle = data[3] + (data[4] << 8);

		if ((bthci->state & (~HCISTATE_WAIT)) == HCISTATE_AUTHENTICATIONREQUESTED && status == HCI_ERRCODE_SUCCESS)
		{
			if (bthci->conn_handle == conn_handle)
			{
				bthci->substate |= HCISUBSTATE_AUTHENTICATIONREQUESTED_AUTHCOMPLETE;
			}
			else
			{
				err = 1;
				dbg_printf(DBGMODE_ERR, "HCI event error, auth complete for wrong conn handle, bthci->conn_handle 0x%04X != conn_handle 0x%04X, ", bthci->conn_handle, conn_handle);
			}
		}
		else if (status != HCI_ERRCODE_SUCCESS)
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, auth complete with error status 0x%02X, ", status);
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, auth complete during wrong state, state 0x%02X, ", bthci->state);
		}
	}
	else if (evt_code == HCISTATE_SETCONNECTIONENCRYPTION)
	{
		uint8_t status = data[2];
		uint16_t conn_handle = data[3] + (data[4] << 8);
		uint8_t encryption = data[5];

		if ((bthci->state & (~HCISTATE_WAIT)) == HCISTATE_AUTHENTICATIONREQUESTED && status == HCI_ERRCODE_SUCCESS)
		{
			if (bthci->conn_handle == conn_handle)
			{
				bthci->substate |= HCISUBSTATE_SETCONNECTIONENCRYPTION_ENCRYPTCHANGE;
				dbg_printf(DBGMODE_DEBUG, "HCI event: encryption change to 0x%02X\r\n", encryption);
			}
			else
			{
				err = 1;
				dbg_printf(DBGMODE_ERR, "HCI event error, encryption change for wrong conn handle, bthci->conn_handle 0x%04X != conn_handle 0x%04X, ", bthci->conn_handle, conn_handle);
			}
		}
		else if (status != HCI_ERRCODE_SUCCESS)
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, encryption change error status 0x%02X, ", status);
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, encryption change during wrong state, state 0x%02X, ", bthci->state);
		}
	}
	else if (evt_code == HCI_EVENT_LINKKEYREQUEST)
	{
		uint8_t* bdaddr = &data[2];
		dbg_printf(DBGMODE_DEBUG, "HCI event: Link Key Request from BD_ADDR ");
		dbg_printf(DBGMODE_DEBUG, "%s\r\n", print_bdaddr(&data[2]));

		if (memcmp(bdaddr, bthci->master_bdaddr, 6) == 0)
		{
			if (bthci->state == HCISTATE_AUTHENTICATIONREQUESTED) {
				bthci->substate |= HCISUBSTATE_AUTHENTICATIONREQUESTED_LINKKEYREQUEST;
			}
			else if (bthci->state == HCISTATE_CONNECTIONREQUEST_HIDCONTROL || bthci->state == HCISTATE_CONNECTIONREQUEST_HIDINTERRUPT) {
				bthci->substate |= HCISUBSTATE_CONNECTIONREQUEST_HID_LINKKEYREQUEST;
			}
			else if (bthci->state == HCISTATE_CONFIGUREREQUEST_HIDCONTROL || bthci->state == HCISTATE_CONFIGUREREQUEST_HIDINTERRUPT) {
				bthci->substate |= HCISUBSTATE_CONFIGUREREQUEST_HID_LINKKEYREQUEST;
			}
			else {
				err = 1;
				dbg_printf(DBGMODE_ERR, "HCI event error, link key requested during wrong state, state 0x%02X, ", bthci->state);
			}
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, link key requested for wrong BD_ADDR");
		}
	}
	else if (evt_code == HCI_EVENT_LINKKEYNOTIFICATION)
	{
		uint8_t* bdaddr = &data[2];
		uint8_t* link_key = &data[2+6];
		dbg_printf(DBGMODE_DEBUG, "HCI event: Link Key Notification, BD_ADDR ");
		dbg_printf(DBGMODE_DEBUG, "%s ", print_bdaddr(bdaddr));
		dbg_printf(DBGMODE_DEBUG, ", Link Key: %s \r\n", print_linkkey(link_key));
		if (memcmp(bdaddr, bthci->master_bdaddr, 6) == 0)
		{
			flashfile_cacheFlush();
			nvm_file_t* f = (nvm_file_t*)flashfilesystem.cache;
			if (memcmp(f->d.fmt.link_key, link_key, 16) != 0) {
				// copy only if different
				memcpy(f->d.fmt.link_key, link_key, 16);
				flashfilesystem.cache_dirty = 1;
			}
			flashfile_cacheFlush();
		}
		else
		{
			err = 1;
			dbg_printf(DBGMODE_ERR, "HCI event error, link key notification for wrong BD_ADDR");
		}
	}
	else
	{
		dbg_printf(DBGMODE_ERR, "Unhandled HCI Event, evt code 0x%02X\r\n", evt_code);
	}

	if (err != 0)
	{
		if (bthci->retries >= 8)
		{
			bthci->state = HCISTATE_UNRECOVERABLE_ERROR;
			dbg_printf(DBGMODE_ERR, "no more retries\r\n");
		}
		else
		{
			dbg_printf(DBGMODE_ERR, "retry %d\r\n", bthci->retries);
			bthci->state &= ~HCISTATE_WAIT;
			bthci->retries++;
		}
	}
}

void HCI_HandleData(BTHCI_t* bthci, uint8_t* data)
{
}

void HCI_Task(BTHCI_t* bthci)
{
	dbgwdg_feed();

	if ((bthci->state & HCISTATE_WAIT) == 0)
	{
		if (bthci->state == HCISTATE_INIT) {
			bthci->state = HCISTATE_RESET;
		}
		else if (bthci->state == HCISTATE_RESET)
		{
			dbg_printf(DBGMODE_TRACE, "HCI_Task, state is HCISTATE_RESET\r\n");

			if (HCI_Command(bthci, HCI_CMD_RESET, hci_tx_buff, 0) == 0) {
				bthci->state |= HCISTATE_WAIT;
				bthci->reset_tmr = systick_1ms_cnt;
			}
			else {
				dbg_printf(DBGMODE_ERR, "Error sending HCI_CMD_RESET\r\n");
			}
		}
		else if (bthci->state == HCISTATE_READBDADDR)
		{
			dbg_printf(DBGMODE_TRACE, "HCI_Task, state is HCISTATE_READBDADDR\r\n");

			if (HCI_Command(bthci, HCI_CMD_READBDADDR, hci_tx_buff, 0) == 0) {
				bthci->state |= HCISTATE_WAIT;
			}
			else {
				dbg_printf(DBGMODE_ERR, "Error sending HCI_CMD_RESET\r\n");
			}
		}
	}
	else
	{
		HCI_State_t s = bthci->state & (~HCISTATE_WAIT);
		if (s == HCISTATE_RESET)
		{
			if ((systick_1ms_cnt - bthci->reset_tmr) > 500) {
				dbg_printf(DBGMODE_ERR, "Error, HCI reset timed out\r\n");
				bthci->state = HCISTATE_READBDADDR;
			}
		}
	}
}

char HCI_Command(BTHCI_t* bthci, uint16_t opcode, uint8_t* params, uint8_t len)
{
	bthci->last_cmd = opcode;

	if (params == 0) {
		params = hci_tx_buff;
	}

	if (bthci->usb_core != 0 && bthci->usbh_dev != 0)
	{
		dbg_printf(DBGMODE_TRACE, "HCI_Command via USB, opcode 0x%04X, length %d\r\n", opcode, len);

		for (int i = len + 1; i >= 2; i--) {
			params[i] = params[i - 2];
		}
		uint16_t* opcode_ptr = (uint16_t*)params;
		*opcode_ptr = opcode;
		USBH_Status status = USBH_Dev_BtHci_Command(bthci->usb_core, bthci->usbh_dev, params, len + 2);
		if (status == USBH_OK) {
			return 0;
		}
		else {
			dbg_printf(DBGMODE_ERR, "Error: HCI_Command via USB failed with status 0x%08X\r\n", status);
			return -1;
		}
	}
	else if (bthci->uart != 0)
	{
		dbg_printf(DBGMODE_TRACE, "HCI_Command via UART, opcode 0x%04X, length %d\r\n", opcode, len);
		// TODO
	}
	else
	{
		dbg_printf(DBGMODE_ERR, "Error: no interface available for HCI Command\r\n");
		return -1;
	}

}

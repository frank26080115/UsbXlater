#ifndef BTSTACK_DEBUG_H_
#define BTSTACK_DEBUG_H_

#include <btstack-config.h>
#include <btstack/src/hci_dump.h>
#include <stdio.h>
#include <utilities.h>

#ifdef ENABLE_LOG_DEBUG
#ifdef HAVE_HCI_DUMP
#define log_debug(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_debug(format, ...)  dbg_printf(DBGMODE_DEBUG, format "\r\n",  ## __VA_ARGS__)
#endif
#else
#define log_debug(...)
#endif

#ifdef ENABLE_LOG_INFO
#ifdef HAVE_HCI_DUMP
#define log_info(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_info(format, ...)  dbg_printf(DBGMODE_INFO, format "\r\n",  ## __VA_ARGS__)
#endif
#else
#define log_info(...)
#endif

#ifdef ENABLE_LOG_ERROR
#ifdef HAVE_HCI_DUMP
#define log_error(format, ...)  hci_dump_log(format,  ## __VA_ARGS__)
#else
#define log_error(format, ...)  dbg_printf(DBGMODE_ERR, format "\r\n",  ## __VA_ARGS__)
#endif
#else
#define log_error(...)
#endif

#endif

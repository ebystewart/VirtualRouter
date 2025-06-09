#ifndef __TCP_IP_NOTIF__
#define __TCP_IP_NOTIF__

#include "utils.h"
#include <stdint.h>
#include "notif.h"

typedef struct interface_ interface_t;
typedef struct intf_nw_prop_ intf_nw_prop_t;

typedef struct intf_notif_data_{

    interface_t *interface;
    intf_nw_prop_t *old_intf_nw_prop;
    uint32_t change_flags;

}intf_notif_data_t;

/* Routines for interface notif chains */
void nfc_intf_register_for_events(nfc_app_cb app_cb);

void nfc_intf_invoke_notification_to_subscribers(interface_t *intf, intf_nw_prop_t *old_intf_nw_props, uint32_t change_flags);

/* structure for wrapping up packet info to application 
   Only application can interpret the payload */
typedef struct pkt_info_{
    uint32_t protocol_no;
    char *pkt;
    uint32_t pkt_size;
    char *pkt_print_buffer;
    uint32_t bytes_written;
}pkt_info_t;

void nfc_register_for_pkt_tracing(uint32_t protocol_no, nfc_app_cb app_cb);

int nfc_pkt_trace_invoke_notif_to_subscribers(uint32_t protocol_no, char *pkt, uint32_t pkt_size, char *pkt_print_buffer);

#endif
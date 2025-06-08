#include "tcpip_notif.h"
#include "notif.h"
#include "net.h"
#include <stdlib.h>
#include <memory.h>

/* create a notification chain for notifying interface configuration change to applications */
static notif_chain_t nfc_intf = {
    "Notif Chain for Interfaces",
    NULL
};


void nfc_intf_invoke_notification_to_subscribers(interface_t *intf, intf_nw_prop_t *old_intf_nw_props, uint32_t change_flags)
{

}
#ifndef __TCP_IP_NOTIF__
#define __TCP_IP_NOTIF__

#include "utils.h"
#include <stdint.h>
#include "net.h"

void nfc_intf_invoke_notification_to_subscribers(interface_t *intf, intf_nw_prop_t *old_intf_nw_props, uint32_t change_flags);

#endif
#include "tcpip_notif.h"
#include "notif.h"
#include "net.h"
#include <stdlib.h>
#include <memory.h>

/* create a notification chain for notifying interface configuration change to applications */
static notif_chain_t nfc_intf = {
    "Notif Chain for Interfaces",
    {NULL, NULL} /* Left, right */
};

/* notification chain for printing appl specific packets for tracing */
static notif_chain_t nfc_print_pkts = {
    "Notification chain for tracing appl pkts",
    {NULL, NULL}
};

/* interface notif chain element do not have any keys */
void nfc_intf_register_for_events(nfc_app_cb app_cb)
{
    notif_chain_elem_t nfce_template;
    memset(&nfce_template, 0, sizeof(notif_chain_elem_t));
    nfce_template.app_cb = app_cb;
    nfce_template.is_key_set = FALSE;

    init_glthread(&nfce_template.glue);
    nfc_register_notif_chain(&nfc_intf, &nfce_template);
}

void nfc_intf_invoke_notification_to_subscribers(interface_t *intf, intf_nw_prop_t *old_intf_nw_props, uint32_t change_flags)
{
    intf_notif_data_t intf_notif_data;
    intf_notif_data.interface = intf;
    intf_notif_data.old_intf_nw_prop = old_intf_nw_props;
    intf_notif_data.change_flags = change_flags;

    nfc_invoke_notif_chain(&nfc_intf, (void *)&intf_notif_data, sizeof(intf_notif_data_t), NULL, 0);
}

void nfc_register_for_pkt_tracing(uint32_t protocol_no, nfc_app_cb app_cb)
{
    notif_chain_elem_t nfce_template;
    memset(&nfce_template, 0, sizeof(notif_chain_elem_t));
    memcpy(&nfce_template.key, (char *)&protocol_no, sizeof(protocol_no));
    nfce_template.key_size = sizeof(protocol_no);
    nfce_template.is_key_set = TRUE;
    nfce_template.app_cb = app_cb;

    init_glthread(&nfce_template.glue);
    nfc_register_notif_chain(&nfc_print_pkts, &nfce_template);

}

int nfc_pkt_trace_invoke_notif_to_subscribers(uint32_t protocol_no, char *pkt, uint32_t pkt_size, char *pkt_print_buffer)
{
    pkt_info_t pkt_info;
    pkt_info.protocol_no = protocol_no;
    pkt_info.pkt = pkt;
    pkt_info.pkt_size = pkt_size;
    pkt_info.pkt_print_buffer = pkt_print_buffer;
    pkt_info.bytes_written = 0;

    nfc_invoke_notif_chain(&nfc_print_pkts, (void *)&pkt_info, sizeof(pkt_info_t), (char *)&protocol_no, sizeof(protocol_no));
    return pkt_info.bytes_written;
}


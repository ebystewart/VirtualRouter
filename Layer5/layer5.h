#ifndef __LAYER5_H__
#define __LYAER5_H__

#include "../tcpip_notif.h"

typedef struct node_ node_t;
typedef struct interface_ interface_t;

typedef struct pkt_notif_data_
{
    node_t *recv_node;
    interface_t *recv_interface;
    char *pkt;
    uint32_t pkt_size;
    uint32_t flags;
    uint32_t protocol_no;
}pkt_notif_data_t;

void promote_pkt_to_layer5(node_t *node, interface_t *recv_intf, char *l5_hdr, uint32_t pkt_size, uint32_t L5_protocol, uint32_t flags);

void tcp_app_register_l2_protocol_interest(uint32_t L5_protocol, nfc_app_cb app_layer_cb);

void tcp_app_register_l3_protocol_interest(uint32_t L5_protocol, nfc_app_cb app_layer_cb);

#endif
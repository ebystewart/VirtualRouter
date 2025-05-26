#ifndef L2SWITCH_H
#define L2SWITCH_H

#include "../net.h"
#include "../graph.h"
#include "../utils.h"
#include "layer2.h"
#include "../comm.h"
#include "../glueThread/glthread.h"
#include <stdio.h>

typedef struct mac_table_{
    glthread_t mac_entries;
}mac_table_t;

typedef struct mac_table_entry_{
    mac_add_t mac;
    char oif_name[IF_NAME_SIZE];
    glthread_t mac_entry_glue;
}mac_table_entry_t;

void l2_switch_recv_frame(interface_t *interface, char *pkt, unsigned int pkt_size);
void l2_switch_forward_frame(node_t *node, interface_t *recv_intf, ethernet_frame_t *eth_frame, unsigned int pkt_size);
void l2_switch_perform_mac_learning(node_t *node, char *src_mac, char *if_name);

void delete_mac_table_entry(mac_table_t *mac_table, char *mac);
mac_table_entry_t *mac_table_lookup(mac_table_t *mac_table, char *mac);
void delete_mac_table_entry(mac_table_t *mac_table, char *mac);



#endif
#ifndef COMM_H
#define COMM_H

#include "graph.h"

typedef struct node_ node_t;
typedef struct interface_ interface_t;

#define MAX_RECEIVE_BUFFER_SIZE 2048U
#define MAX_SEND_BUFFER_SIZE 2048U
#define MAX_PACKET_BUFFER_SIZE  2048U

void init_udp_socket(node_t *node);

//void network_start_pkt_receiver_thread(graph_t *topo);

int send_pkt_out(char *pkt, unsigned int pkt_size, interface_t *interface);

#endif
#include <stdio.h>
#include "../Layer3/layer3.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "tcpconst.h"

extern void demote_packet_to_layer3(node_t *node, char *pkt, unsigned int size, int protocol_number, unsigned int dest_ip_address);


void layer5_ping_fn(node_t *node, char *dst_ip_addr)
{
    unsigned int ip_addr_int;
    printf("%s: Src node - %s, Ping IP - %s\n", __FUNCTION__, node->node_name, dst_ip_addr);
    inet_pton(AF_INET, dst_ip_addr, &ip_addr_int);
    ip_addr_int = htonl(ip_addr_int);

    demote_packet_to_layer3(node, NULL, 0, ICMP_PRO, ip_addr_int);
}

void layer5_ero_ping_fn(node_t *node, char *dst_ip_addr, char *ero_ip_addr)
{

}
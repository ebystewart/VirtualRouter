#include <stdio.h>
#include "../Layer3/layer3.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include "tcpconst.h"
#include "../Layer3/ip.h"
#include "../net.h"

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
    /*Prepare the payload and push it down to the network layer.
     *      The payload shall be inner ip hdr*/
    ip_hdr_t *inner_ip_hdr = calloc(1, sizeof(ip_hdr_t));
    initialize_ip_pkt(inner_ip_hdr);
    inner_ip_hdr->total_length = sizeof(ip_hdr_t)/4;
    inner_ip_hdr->protocol = ICMP_PRO;

    unsigned int addr_int = 0;
    inet_pton(AF_INET, NODE_LO_ADDRESS(node), &addr_int);
    addr_int = htonl(addr_int);
    inner_ip_hdr->src_ip = addr_int;

    addr_int = 0;
    inet_pton(AF_INET, dst_ip_addr, &addr_int);
    addr_int = htonl(addr_int);
    inner_ip_hdr->dst_ip = addr_int;

    addr_int = 0;
    inet_pton(AF_INET, ero_ip_addr, &addr_int);
    addr_int = htonl(addr_int);

    demote_packet_to_layer3(node, (char *)inner_ip_hdr,
            inner_ip_hdr->total_length * 4,
            IP_IN_IP, addr_int);
    free(inner_ip_hdr);
}
#include "tcp_public.h"
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <netdb.h> /*for struct hostent*/

#define INGRESS_INTF_NAME "eth0/1"
#define DEST_IP_ADDR "122.1.1.3"
#define SRC_NODE_UDP_PORT_NO 40000

static char send_buffer[MAX_PACKET_BUFFER_SIZE];

int main(int argc, char **argv)
{
    uint32_t totalDataLen;
    int rc;
    uint32_t nPktsSent = 0U;
    struct sockaddr_in dest_addr;
    struct hostent *host = (struct hostent *)gethostbyname("127.0.0.1");
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = *((struct in_addr *)host->h_addr);
    dest_addr.sin_port = SRC_NODE_UDP_PORT_NO;

    int udp_sock_fd;
    udp_sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(udp_sock_fd == -1){
        printf("%s: UDP socket creation failed with error %d\n", __FUNCTION__, errno);
        return -1;
    }
    memset(send_buffer, 0, MAX_PACKET_BUFFER_SIZE);
    /* First info would be a string (16 Bytes long) holding the name of the interface */
    strncpy(send_buffer, INGRESS_INTF_NAME, IF_NAME_SIZE);

    /* Offset first 16 Bytes, then starts the ethernet packet */
    ethernet_frame_t *eth_pkt = (ethernet_frame_t *)(send_buffer + IF_NAME_SIZE);
    /* Source and destination mac addresses should be of broadcast type, since IP is going to be loopback */
    layer2_fill_with_broadcast_mac(eth_pkt->dst_mac.mac_addr);
    layer2_fill_with_broadcast_mac(eth_pkt->src_mac.mac_addr);

    eth_pkt->type = ETH_IP;
    SET_COMMON_ETH_FCS(eth_pkt, 20U, 0); //set FCS=0 after 20 Bytes payload; 20 Bytes being basic IP pkt size

    /* Prepare pseudo Ip header */
    ip_hdr_t *ip_hdr = (ip_hdr_t *)(eth_pkt->payload);
    initialize_ip_pkt(ip_hdr);
    ip_hdr->protocol = ICMP_PRO;
    ip_hdr->dst_ip = ip_addr_p_to_n(DEST_IP_ADDR);

    totalDataLen = ETH_HDR_SIZE_EXCL_PAYLOAD + 20U + IF_NAME_SIZE;

    while(1)
    {
        rc = sendto(udp_sock_fd, send_buffer, totalDataLen, 0U, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
        nPktsSent++;
        printf("%s: No. of Bytes sent - %d, Pkt Number %d\n", rc, nPktsSent);
        usleep(100000); /* 10 pkts/sec */
    }
    close(udp_sock_fd);
    return 0;
}
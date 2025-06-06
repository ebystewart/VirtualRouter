#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "tcp_public.h"

static char string_buffer[32];

static void init_string_buffer(void){
    memset(string_buffer, 0, sizeof(string_buffer));
}

static char *string_ethernet_hdr_type(unsigned short type){

    char *proto_srt = NULL;
    init_string_buffer();
    switch(type){
        case ETH_IP:
        {
            strncpy(string_buffer, "ETH_IP", strlen("ETH_IP"));
            break;
        }
        case ARP_MSG:
        {
            strncpy(string_buffer, "ARP_MSG", strlen("ARP_MSG"));
            break;
        }
        default:
            break;
    }

    return string_buffer;
}

static char *string_arp_hdr_type(int type)
{
    init_string_buffer();
    switch(type){
        case ARP_BROAD_REQ:
        {
            strncpy(string_buffer, "ARP_BROAD_REQ", strlen("ARP_BROAD_REQ"));
            break;
        }
        case ARP_REPLY:
        {
            strncpy(string_buffer, "ARP_REPLY", strlen("ARP_REPLY"));
            break;
        }
        default:
            ;
    }
    return string_buffer;
}

static char *string_ip_hdr_protocol_val(uint8_t type)
{
    init_string_buffer();
    switch(type){
        case ICMP_PRO:
        {
            strncpy(string_buffer, "ICMP_PRO", strlen("ICMP_PRO"));
            break;
        }
        default:
            return NULL;
    }
    return string_buffer;
}

static int tcp_dump_appln_hdr_protocol_icmp(char *buff, char *appln_data, uint32_t pkt_size)
{

    return 0;
}

static int tcp_dump_ip_hdr(char *buff, ip_hdr_t *ip_hdr, uint32_t pkt_size)
{
    int rc = 0;
    char ip1[16];
    char ip2[16];

    ip_addr_n_to_p(ip_hdr->src_ip, ip1);
    ip_addr_n_to_p(ip_hdr->dst_ip, ip2);

    rc += sprintf(buff + rc, "IP Hdr: ");
    rc += sprintf(buff + rc, "TL: %dB PRO: %s %s -> %s ttl: %d\n", IP_HDR_LEN_IN_BYTES(ip_hdr), string_ip_hdr_protocol_val(ip_hdr->protocol),\
                           ip1, ip2, ip_hdr->ttl);

    switch(ip_hdr->protocol){
        case ICMP_PRO:
        {
            rc += tcp_dump_appln_hdr_protocol_icmp(buff + rc, IP_PKT_PAYLOAD_PTR(ip_hdr),IP_PKT_PAYLOAD_SIZE(ip_hdr));
            break;
        }
        default:
            break;
    }

    return rc;
}

static int tcp_dump_arp_hdr(char *buff, arp_packet_t *arp_hdr, uint32_t pkt_size)
{
    int rc = 0;
    char ip1[16];
    char ip2[16];

    ip_addr_n_to_p(arp_hdr->src_ip, ip1);
    ip_addr_n_to_p(arp_hdr->dst_ip, ip2);

    rc += sprintf(buff + rc, "ARP Hdr: ");
    rc += sprintf(buff + rc, "Arp Type: %s %02x:%02x:%02x:%02x:%02x:%02x -->"
                                        "%02x:%02x:%02x:%02x:%02x:%02x %s --> %s\n", string_arp_hdr_type(arp_hdr->op_code),\
                                        arp_hdr->src_mac.mac_addr[0],
                                        arp_hdr->src_mac.mac_addr[1],
                                        arp_hdr->src_mac.mac_addr[2],
                                        arp_hdr->src_mac.mac_addr[3],
                                        arp_hdr->src_mac.mac_addr[4],
                                        arp_hdr->src_mac.mac_addr[5],
                                        arp_hdr->dst_mac.mac_addr[0],
                                        arp_hdr->dst_mac.mac_addr[1],
                                        arp_hdr->dst_mac.mac_addr[2],   
                                        arp_hdr->dst_mac.mac_addr[3],
                                        arp_hdr->dst_mac.mac_addr[4],
                                        arp_hdr->dst_mac.mac_addr[5], ip1, ip2);

    return rc;
}

static int tcp_dump_ethernet_hdr(char *buff, ethernet_frame_t *eth_hdr, uint32_t pkt_size)
{
    int rc = 0;
    vlan_ethernet_frame_t *vlan_eth_hdr = NULL;
    uint32_t payload_size;
    unsigned short type;

    payload_size = pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(eth_hdr);

    vlan_8021q_hdr_t * vlan_8021q_hdr = is_pkt_vlan_tagged(eth_hdr);
    if(vlan_8021q_hdr){
        vlan_eth_hdr = (vlan_ethernet_frame_t *)eth_hdr;
    }

    type = vlan_8021q_hdr ? vlan_eth_hdr->type : eth_hdr->type;

    rc += sprintf(buff + rc, "Eth Hdr: ");
    rc += sprintf(buff + rc, "%02x:%02x:%02x:%02x:%02x:%02x -->"
                             "%02x:%02x:%02x:%02x:%02x:%02x %-4s Vlan: %d PL: %dB\n",
                            eth_hdr->src_mac.mac_addr[0],
                            eth_hdr->src_mac.mac_addr[1],
                            eth_hdr->src_mac.mac_addr[2],
                            eth_hdr->src_mac.mac_addr[3],
                            eth_hdr->src_mac.mac_addr[4],
                            eth_hdr->src_mac.mac_addr[5],
                        
                            eth_hdr->dst_mac.mac_addr[0],
                            eth_hdr->dst_mac.mac_addr[1],
                            eth_hdr->dst_mac.mac_addr[2],
                            eth_hdr->dst_mac.mac_addr[3],
                            eth_hdr->dst_mac.mac_addr[4],
                            eth_hdr->dst_mac.mac_addr[5],
                            string_ethernet_hdr_type(type),

                            vlan_8021q_hdr ? GET_802_1Q_VLAN_ID(vlan_8021q_hdr) : 0, payload_size);
    
    switch(type){
        case ETH_IP:
        {
            rc += tcp_dump_ip_hdr(buff + rc, (ip_hdr_t *)GET_ETHERNET_HDR_PAYLOAD(eth_hdr), payload_size);
            break;
        }
        case ARP_MSG:
        {
            rc += tcp_dump_arp_hdr(buff + rc, (arp_packet_t *)GET_ETHERNET_HDR_PAYLOAD(eth_hdr), payload_size);
            break;
        }
        default:
            break;
    }

    return rc;
}

static FILE *initialize_node_log_file(node_t *node)
{
    char file_name[32];
    memset(file_name, 0, sizeof(file_name));
    sprintf(file_name, "logs/%s.txt", node->node_name);

    FILE *fptr = open(file_name, "w");
    if(!fptr){
        printf("%s: Could not create node log file %s, errorcode %d\n", file_name, errno);
        return 0;
    }
    return fptr;
}

static FILE *initialize_interface_log_file(interface_t *intf)
{
    char file_name[64];
    memset(file_name, 0, sizeof(file_name));
    node_t *node = intf->att_node;
    sprintf(file_name, "logs/%s-%s.txt", node->node_name, intf->if_name);

    FILE *fptr = open(file_name, "w");
    if(!fptr){
        printf("%s: Could not create interface log file %s, errorcode %d\n", file_name, errno);
        return 0;
    }
    return fptr;
}

void tcp_ip_init_node_log_info(node_t *node){

    log_t *log_info = &node->log_info;
    log_info->all       = FALSE;
    log_info->recv      = FALSE;
    log_info->send      = FALSE;
    log_info->is_stdout = FALSE;
    log_info->l3_fwd    = FALSE;
    log_info->log_file = initialize_node_log_file(node); 
}

void tcp_ip_set_all_log_info_params(log_t *log_info, bool_t status){
    log_info->all       = status;
    log_info->recv      = status;
    log_info->send      = status;
    log_info->l3_fwd    = status;
}

void tcp_ip_init_intf_log_info(interface_t *intf){

    log_t *log_info = &intf->log_info;
    log_info->all       = FALSE;
    log_info->recv      = FALSE;
    log_info->send      = FALSE;
    log_info->is_stdout = FALSE;
    log_info->l3_fwd    = FALSE;
    log_info->log_file = initialize_interface_log_file(intf); 
}
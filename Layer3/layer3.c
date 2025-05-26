#include "layer3.h"
#include <stdio.h>
#include <arpa/inet.h>
#include "ip.h"
#include "utils.h"
#include <string.h>
#include "../tcpconst.h"

typedef bool bool_t;

/* Routing table APIs */

void init_rt_table(rt_table_t **rt_table){
    *rt_table = calloc(1, sizeof(rt_table_t));
    init_glthread(&((*rt_table)->route_list));
}

static bool_t rt_table_entry_add(rt_table_t *rt_table, l3_route_t *l3_route){
    l3_route_t *l3_route_old = rt_table_lookup(rt_table, l3_route->dest_ip, l3_route->mask_dest_ip);

    if(l3_route_old && IS_L3_ROUTES_EQUAL(l3_route_old, l3_route)){
        return FALSE;
    }
    if(l3_route_old){
        delete_rt_table_entry(rt_table, l3_route->dest_ip, l3_route->mask_dest_ip);
    }
    init_glthread(&l3_route->rt_glue);
    glthread_add_next(&rt_table->route_list, &l3_route->rt_glue);
    return TRUE;
}

static bool_t l3_is_direct_route(l3_route_t *l3_route)
{
    return l3_route->is_direct;
}

bool_t is_layer3_local_delivery(node_t *node, unsigned int dst_ip)
{
    /* Check if dst_ip matches with any of the interface IP or Node IP (loopback) of the router */
    /* Checking for loopback address match */
    char dst_ip_str[16];
    dst_ip_str[15] = '\0';
    //dst_ip = htonl(dst_ip);
    inet_ntop(AF_INET, &dst_ip, dst_ip_str, 16);

    printf("%s: INFO - The destination IP %s\n", __FUNCTION__, dst_ip_str);
    if(strncmp(NODE_LO_ADDRESS(node), dst_ip_str, 16U) == 0U)
    {
        printf("%s: INFO - LO address %s matches with the destination IP %s\n", __FUNCTION__, NODE_LO_ADDRESS(node), dst_ip_str);
        return TRUE;
    }

    /* checking for interface IP address match */
    interface_t *intf;
    char *intf_addr = NULL;
    unsigned int idx = 0U;
    for(; idx < MAX_IF_PER_NODE; idx++){
        intf = node->intf[idx];
        if(!intf || intf->intf_nw_prop.is_ipaddr_config == FALSE)
            continue;
        intf_addr = IF_IP(intf);//intf->intf_nw_prop.ip_addr.ip_addr;
        if(strncmp(intf_addr, dst_ip_str, 16U) == 0U){
            printf("%s: INFO - Interface %s's IP address matches with the destination IP\n", __FUNCTION__, intf->if_name);
            return TRUE;
        }
    } 
    return FALSE;
}

void rt_table_add_direct_route(rt_table_t *rt_table, char *dst_ip, char mask){
    rt_table_add_route(rt_table, dst_ip, mask, 0, 0);
}

void rt_table_add_route(rt_table_t *rt_table, char *dst_ip, char mask, char *gw_ip, char *oif){

    unsigned int dst_ip_int = 0U;
    char dst_str_with_mask[16];

    printf("%s: Info - Destination Ip is %s\n", __FUNCTION__, dst_ip);
    apply_mask(dst_ip, mask, dst_str_with_mask);
    printf("%s: Info - Masked destination IP (subnet Id) is %s\n", __FUNCTION__, dst_str_with_mask);
    inet_pton(AF_INET, dst_str_with_mask, &dst_ip_int);
    dst_ip_int = ntohl(dst_ip_int);//
    printf("%s: Info - IP as integer is %d\n",__FUNCTION__, dst_ip_int);
    l3_route_t *l3_route = l3rib_lookup_lpm(rt_table, dst_ip_int);
    /* Assert if route entry already exist */
    assert(!l3_route);

    l3_route = calloc(1, sizeof(l3_route_t));
    strncpy(l3_route->dest_ip, dst_str_with_mask, 16);
    l3_route->dest_ip[15] = '\0';
    l3_route->mask_dest_ip = mask;

    if(!gw_ip && !oif)
        l3_route->is_direct = TRUE;
    else
        l3_route->is_direct = FALSE;

    if(gw_ip && oif){
        strncpy(l3_route->gw_ip, gw_ip, 16);
        l3_route->dest_ip[15] = '\0';
        strncpy(l3_route->oif, oif, IF_NAME_SIZE);
        l3_route->oif[IF_NAME_SIZE - 1] = '\0';
    }

    if(!rt_table_entry_add(rt_table, l3_route)){
        printf("Error: Route %s/%d installation failed \n", dst_str_with_mask, mask);
        free(l3_route);
    }
}

l3_route_t *rt_table_lookup(rt_table_t *rt_table, char *ip_addr, char mask){

    l3_route_t *l3_route;
    glthread_t *curr;

    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr){
        l3_route = rt_glue_to_l3_route(curr);
        if((strncmp(l3_route->dest_ip, ip_addr, 16) == 0U) && (l3_route->mask_dest_ip == mask)){
            return l3_route;
        }
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
}

/* Lookup routing table using longest prefix match */
l3_route_t *l3rib_lookup_lpm(rt_table_t *rt_table, uint32_t dst_ip){

    l3_route_t *l3_route = NULL;
    l3_route_t *lpm_l3_route = NULL;
    l3_route_t *default_l3_route = NULL;
    glthread_t *curr = NULL;

    char subnet[16];
    char dst_ip_str[16];
    char longest_mask = 0;

    dst_ip = htonl(dst_ip);
    inet_ntop(AF_INET, &dst_ip, dst_ip_str, 16);
    dst_ip_str[15] = '\0';

    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr){

        l3_route = rt_glue_to_l3_route(curr);
        memset(subnet, 0, 16);
        apply_mask(dst_ip_str, l3_route->mask_dest_ip, subnet);
        printf("%s:subnet id of %s with mask %d is %s\n", __FUNCTION__, dst_ip_str, l3_route->mask_dest_ip, subnet);

        if((strncmp("0.0.0.0", l3_route->dest_ip, 16) == 0U) && (l3_route->mask_dest_ip == 0U)){
            default_l3_route = l3_route;
        }
        else if(strncmp(subnet, l3_route->dest_ip, strlen(subnet)) == 0U)
        { 
            printf("%s:Comparing %s with %s\n", __FUNCTION__, subnet, l3_route->dest_ip);
            if(l3_route->mask_dest_ip > longest_mask)
            {
                longest_mask = l3_route->mask_dest_ip;
                lpm_l3_route = l3_route;
            }
        }

    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);

    return (lpm_l3_route ? lpm_l3_route : default_l3_route);
}

void delete_rt_table_entry(rt_table_t *rt_table, char *ip_addr, char mask)
{
    char dst_ip_str_with_mask[16];
    apply_mask(ip_addr, mask, dst_ip_str_with_mask);

    l3_route_t *l3_route = rt_table_lookup(rt_table, dst_ip_str_with_mask, mask);
    if(!l3_route)
        return;
    remove_glthread(&l3_route->rt_glue);
    free(l3_route);
}

void clear_rt_table(rt_table_t *rt_table)
{
    l3_route_t *l3_route;
    glthread_t *curr;

    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr){
        l3_route = rt_glue_to_l3_route(curr);
        remove_glthread(&l3_route->rt_glue);
        free(l3_route);
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
}

void dump_rt_table(rt_table_t *rt_table){
    l3_route_t *l3_route;
    glthread_t *curr;

    ITERATE_GLTHREAD_BEGIN(&rt_table->route_list, curr){
        l3_route = rt_glue_to_l3_route(curr);
        printf("\t%-18s %-4d %-18s %s\n",
                l3_route->dest_ip, l3_route->mask_dest_ip, 
                l3_route->is_direct ? "NA" : l3_route->gw_ip,
                l3_route->is_direct ? "NA" : l3_route->oif);
    }ITERATE_GLTHREAD_END(&rt_table->route_list, curr);
}

/* Packet promotion APIs */
static void layer3_pkt_recv_from_top(node_t *node, char *pkt, unsigned int size, int protocol_number, unsigned int dest_ip_address)
{
    ip_hdr_t ip_hdr;
    initialize_ip_pkt(&ip_hdr);
    /* Now fill the non-default fields */
    ip_hdr.protocol = protocol_number;

    unsigned int lo_addr_int = 0U;
    inet_pton(AF_INET, NODE_LO_ADDRESS(node), &lo_addr_int);
    lo_addr_int = htonl(lo_addr_int);

    ip_hdr.src_ip = lo_addr_int;
    ip_hdr.dst_ip = dest_ip_address;
    ip_hdr.total_length = (short)ip_hdr.hdr_len + (short)(size/4) + (short)((size%4)?1:0);

    l3_route_t *l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), ip_hdr.dst_ip);
    if(!l3_route){
        printf("%s: No L3 route found on node %s\n", __FUNCTION__, node->node_name);
        return;
    }
    printf("%s: Info - L3 route found on %s\n", __FUNCTION__, node->node_name);
    char *new_pkt = NULL;
    unsigned int new_pkt_size = 0U;
    new_pkt_size = ip_hdr.total_length * 4U;
    new_pkt = calloc(1, MAX_PACKET_BUFFER_SIZE);
    memcpy(new_pkt, (char *)&ip_hdr, (ip_hdr.hdr_len* 4U));

    if(pkt && size){
        memcpy(new_pkt + (ip_hdr.hdr_len * 4U), pkt, size);
    }
    /* Now resolve next hop */
    bool_t is_direct_route = l3_is_direct_route(l3_route);
    unsigned int next_hop_ip;
    if(!is_direct_route){
        /* Case 1 - L3 forwarding */
        inet_pton(AF_INET, l3_route->gw_ip, &next_hop_ip);
        next_hop_ip = htonl(next_hop_ip);
        printf("%s: Info - Forward the pkt to the found route\n", __FUNCTION__);
    }
    else{
        /* Case 2: Direct host (same subnet) delivery case */
        /* Case 4: Self ping case */
        /* The Data Link Layer will differentiate between case 2 and 4 and take appropriate action */
        next_hop_ip = dest_ip_address;
        printf("%s: Info - IP belongs to the same subnet or to itself\n", __FUNCTION__);
    }
    char *shifted_pkt_buffer = pkt_buffer_shift_right(new_pkt, new_pkt_size, MAX_PACKET_BUFFER_SIZE);
    printf("%s: Info - pkt size is %u\n", __FUNCTION__, new_pkt_size);

    demote_pkt_to_layer2(node, next_hop_ip, is_direct_route? 0 : l3_route->oif, shifted_pkt_buffer, new_pkt_size, ETH_IP);
    free(new_pkt);
}

/*An API to be used by L4 or L5 to push the pkt down the TCP/IP
 * stack to layer 3*/
void demote_packet_to_layer3(node_t *node, char *pkt, unsigned int size, int protocol_number, /*L4 or L5 protocol type*/
                        unsigned int dest_ip_address){

    layer3_pkt_recv_from_top(node, pkt, size, protocol_number, dest_ip_address);
}

static void layer3_ip_pkt_recv_from_bottom(node_t *node, interface_t *interface, ip_hdr_t *pkt, unsigned int pkt_size)
{
    char *l4_hdr;
    char *l5_hdr;
    char dst_ip_addr[16];

    ip_hdr_t *ip_hdr = pkt;
    unsigned int dst_ip = htonl(ip_hdr->dst_ip);
    inet_ntop(AF_INET, &dst_ip, dst_ip_addr, 16);
    printf("%s: Info - pkt size is %u and dst ip is %s\n", __FUNCTION__, pkt_size, dst_ip_addr);

    /* Implement layer3 forwarding */
    l3_route_t * l3_route = l3rib_lookup_lpm(NODE_RT_TABLE(node), ip_hdr->dst_ip);
    if(!l3_route){
        /* router doesn't have a route configured to forward. Drop the pkt */
        printf("%s: Router %s cannot route IP %s\n", __FUNCTION__, node->node_name, dst_ip_addr);
        return;
    }
    /* L3 route exists */
    /* Case #1: Pkt is destined to self */
    /* Case #2: Pkt is destined to a host machine connected to directly attached subnet */
    /* Case #3: Pkt is to be forwarded to the next router */

    if(l3_is_direct_route(l3_route)){
        /* Case #1: Local delivery 
            The destination IP should match with any of the IP of the local interfaces 
            of the router or the loopback address
        */
       printf("%s: INFO - The L3 route configured is direct\n", __FUNCTION__);
       if(is_layer3_local_delivery(node, dst_ip)){
            printf("%s: INFO - LO or Intf address matches with the destination IP\n", __FUNCTION__);
            l4_hdr = (char *)IP_PKT_PAYLOAD_PTR(ip_hdr);
            l5_hdr = l4_hdr;
            switch(ip_hdr->protocol){
                case ICMP_PRO:
                {
                    printf("IP_Address %s, Ping Success\n", dst_ip_addr);
                    break;
                }
                default:
                    ;
            }
            return;
       }
       printf("%s: INFO - No direct match with destination IP\n", __FUNCTION__);
       /* Case #2: Destination IP address is in one of the sub-net connected to the router
          Do L2 routing  */

        demote_pkt_to_layer2(node, 0, NULL, (char *)ip_hdr, pkt_size, ETH_IP);
        return;
    }
    /* Case #3 : L3 forwarding case */
    ip_hdr->ttl--;
    if(ip_hdr->ttl == 0){
        printf("Pkt dropped to TTL of 0\n");
        return;
    }
    unsigned int next_hop_ip;
    inet_pton(AF_INET, l3_route->gw_ip, &next_hop_ip);
    next_hop_ip = htonl(next_hop_ip);
    demote_pkt_to_layer2(node, next_hop_ip, l3_route->oif, (char *)ip_hdr, pkt_size, ETH_IP);
}

static void layer3_pkt_recv_from_bottom(node_t *node, interface_t *interface, char *pkt, unsigned int pkt_size, int L3_protocol_type)
{
    switch(L3_protocol_type){
        case ETH_IP:
        case IP_IN_IP:
        {
            printf("%s: Info - pkt size is %u\n", __FUNCTION__, pkt_size);
            layer3_ip_pkt_recv_from_bottom(node, interface, (ip_hdr_t *)pkt, pkt_size);
            break;
        }
        default:
            ;
    }
}

/* A public API to be used by L2 or other lower Layers to promote
 * pkts to Layer 3 in TCP IP Stack*/
void promote_pkt_to_layer3(node_t *node,                /*Current node on which the pkt is received*/
                      interface_t *interface,           /*ingress interface*/
                      char *pkt, unsigned int pkt_size, /*L3 payload*/
                      int L3_protocol_number){          /*obtained from eth_hdr->type field*/

    layer3_pkt_recv_from_bottom(node, interface, pkt, pkt_size, L3_protocol_number);
}

/* Extern APIs which are to be defined in other layers*/
extern void
promote_pkt_to_layer4(node_t *node, interface_t *recv_intf, 
                      char *l4_hdr, unsigned int pkt_size,
                      int L4_protocol_number);

extern void
promote_pkt_to_layer5(node_t *node, interface_t *recv_intf, 
                      char *l5_hdr, unsigned int pkt_size,
                      int L5_protocol_number);


/* Packet Demotion APIs */
/* import function from layer 2 */
extern void
demote_pkt_to_layer2(node_t *node,
                     unsigned int next_hop_ip,
                     char *outgoing_intf, 
                     char *pkt, unsigned int pkt_size,
                     int protocol_number);
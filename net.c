#include <stdio.h>
#include "net.h"
#include "graph.h"
#include "glueThread/glthread.h"
#include <arpa/inet.h>
#include <string.h>
#include "utils.h"


static unsigned int hash_code(void *ptr, unsigned int size);

/*Just some Random number generator*/
static unsigned int hash_code(void *ptr, unsigned int size){
    unsigned int value=0, i =0;
    char *str = (char*)ptr;
    while(i < size)
    {
        value += *str;
        value*=97;
        str++;
        i++;
    }
    return value;
}  

/*Heuristics, Assign a unique mac address to interface*/
void interface_assign_mac_address(interface_t *interface){

    unsigned int hash_code_val = 0U;
    node_t *node = interface->att_node;
    if(!node)
        return;
    hash_code_val = hash_code(node->node_name, NODE_NAME_SIZE);
    hash_code_val *= hash_code(interface->if_name, IF_NAME_SIZE); 
    memset(IF_MAC(interface), 0, sizeof(IF_MAC(interface)));
    memcpy(IF_MAC(interface), (char *)&hash_code_val, sizeof(unsigned int));
    IF_MAC(interface)[4] = (0xFFU) & hash_code(node->node_name, NODE_NAME_SIZE);
    IF_MAC(interface)[5] = (0xFFU) & hash_code(interface->if_name, IF_NAME_SIZE); 
}

bool_t node_set_loopback_address(node_t *node, char *ip_addr)
{
    assert(ip_addr);
    //char addr[16];
    node->node_nw_prop.is_lb_addr_config = TRUE;
    strncpy(NODE_LO_ADDRESS(node), ip_addr, 16);
    NODE_LO_ADDRESS(node)[15] = '\0';

    /* Add it as direct route in routing table */
    rt_table_add_direct_route(NODE_RT_TABLE(node), NODE_LO_ADDRESS(node), 32);
    return TRUE;
}

bool_t node_set_intf_ip_address(node_t *node, char *local_if, char *ip_addr, char mask)
{
    interface_t *interface = get_node_intf_by_name(node, local_if);
    if(!interface)
        assert(0); // otherwise assert(!interface)
    strncpy(IF_IP(interface), ip_addr, 16);
    IF_IP(interface)[15] = '\0';
    interface->intf_nw_prop.is_ipaddr_config = TRUE;
    interface->intf_nw_prop.mask = mask;

    /* Add it as direct route in routing table */
    rt_table_add_direct_route(NODE_RT_TABLE(node), ip_addr, mask);
    //rt_table_add_direct_route(node->node_nw_prop.rt_table, ip_addr, mask);
    return TRUE;
}

bool_t node_unset_intf_ip_address(node_t *node, char *local_if)
{
    return TRUE;
}

void dump_nw_graph(graph_t *graph)
{
    unsigned int i;
    node_t *node;
    glthread_t *curr;
    interface_t *interface;

    printf("Topology Name = %s\n", graph->topology_name);

    ITERATE_GLTHREAD_BEGIN(&graph->node_list, curr){
        node = graph_glue_to_node(curr);
        dump_node_nw_props(node);
        for(i = 0U; i < MAX_IF_PER_NODE; i++)
        {
            interface = node->intf[i];
            //printf("slot %u, interface pointer %x\n", i, interface);
            if(interface != NULL)
                dump_intf_props(interface);
        }
    }ITERATE_GLTHREAD_END(&graph->node_list, curr);
}

void dump_node_nw_props(node_t * node)
{
    printf("Node Name: %s\n", node->node_name);
    if(node->node_nw_prop.is_lb_addr_config){
        printf("\t Lo Address: %s/32\n", NODE_LO_ADDRESS(node));
    }
}

void dump_intf_props(interface_t *interface)
{
    dump_interface(interface);

    printf("\t IF Status : %s\n", IF_IS_UP(interface) ? "UP" : "DOWN");

    if(interface->intf_nw_prop.is_ipaddr_config){
        printf("\t IP Addr = %s/%u", IF_IP(interface), interface->intf_nw_prop.mask);
        printf("\t MAC = %x:%x:%x:%x:%x:%x\n", (0xFFU & IF_MAC(interface)[0]), (0xFFU & IF_MAC(interface)[1]), (0xFFU & IF_MAC(interface)[2]),
        (0xFFU & IF_MAC(interface)[3]), (0xFFU & IF_MAC(interface)[4]), (0xFFU & IF_MAC(interface)[5]));
    }
    else{
        printf("\t l2 mode = %s", intf_l2_mode_str(IF_L2_MODE(interface)));
        printf("\t vlan membership : ");
        int i = 0;
        for(; i < MAX_VLAN_MEMBERSHIP; i++){
           if(interface->intf_nw_prop.vlans[i]){
               printf("%u  ", interface->intf_nw_prop.vlans[i]);
           }
        }
        printf("\n");
    }
}

void dump_node_interface_stats(node_t *node)
{
    interface_t *interface;
    uint8_t idx;

    for(idx = 0U; idx < MAX_IF_PER_NODE; idx++){
        interface = node->intf[idx];
        if(!interface)
            return;
        dump_interface_stats(interface);
    }
}

void dump_interface_stats(interface_t *interface)
{
    printf("\t%s\n", interface->if_name);
    printf("\tTx Statistics: %u\n", interface->intf_nw_prop.pkt_sent);
    printf("\tRx Statistics: %u\n", interface->intf_nw_prop.pkt_recv);
}

char *pkt_buffer_shift_right(char *pkt, unsigned int pkt_size, unsigned int total_buffer_size)
{
    char *dest;
    dest = pkt + total_buffer_size - pkt_size;
    memcpy(dest, pkt, pkt_size);
    memset(pkt, 0, pkt_size);
    return dest;
}

unsigned int ip_addr_p_to_n(char *ip_addr)
{
    unsigned int  binary_prefix;
    inet_pton(AF_INET, ip_addr, &binary_prefix);
    binary_prefix = htonl(binary_prefix);
    return binary_prefix;
}

void ip_addr_n_to_p(unsigned int ip_addr, char *ip_addr_str)
{
    //char *out = ip_addr_str;
    //memset(out, 0, 16);
    memset(ip_addr_str, 0, 16);
    ip_addr = htonl(ip_addr);
    inet_ntop(AF_INET, &ip_addr, ip_addr_str, 16);
    ip_addr_str[15] = '\0';

}

/* returns the local interface of the node which is configured with the subnet in which  "ip-addr" lies */
interface_t *node_get_matching_subnet_interface(node_t *node, char *ip_addr)
{
    interface_t *intf = NULL;
    unsigned int i = 0U;
    //unsigned int mask_length;
    char *intf_addr = NULL;
    char mask;
    char intf_subnet[16];
    char subnet2[16];

    for(; i < MAX_IF_PER_NODE; i++){
        intf = node->intf[i];
        printf("Info: Interface %d is configured as %s\n", i, intf->if_name);
        if(!intf)
            return NULL;
        if(intf->intf_nw_prop.is_ipaddr_config == FALSE)
            continue;
        
        intf_addr = IF_IP(intf);
        mask = intf->intf_nw_prop.mask;
#if 0      
        if((node->intf[i] != NULL) && (node->intf[i]->intf_nw_prop.is_ipaddr_config == TRUE))
        {
            mask_length = sizeof(ip_addr) - (node->intf[i]->intf_nw_prop.mask / 8U);
            if(strncmp(node->intf[i]->intf_nw_prop.ip_addr.ip_addr, ip_addr, mask_length) == 0U)
            {

            }
        }
#endif
        memset(intf_subnet, 0, 16);
        memset(subnet2, 0, 16);
        apply_mask(intf_addr, mask, intf_subnet);
        printf("Info: subnet of IP %s is %s\n", intf_addr, intf_subnet);
        apply_mask(ip_addr, mask, subnet2);
        printf("Info: subnet of IP %s is %s\n", ip_addr, subnet2);
        if(strncmp(intf_subnet, subnet2, 16) == 0U){
            return intf;
        }
    }
}
/* only for ACCESS mode */
unsigned int get_access_intf_operating_vlan_id(interface_t *interface)
{
    unsigned int vlan_id = 0U;
    if(IF_L2_MODE(interface) == ACCESS){
        vlan_id = interface->intf_nw_prop.vlans[0];
    }
    return vlan_id;
}

/* only for TRUNK mode */
bool_t is_trunk_intf_vlan_enabled(interface_t *interface, unsigned int vlan_id)
{
    unsigned int i = 0U;
    if(IF_L2_MODE(interface) == TRUNK){
        for(; i < MAX_VLAN_MEMBERSHIP; i++){
            if(interface->intf_nw_prop.vlans[i] == vlan_id){
                return TRUE;
            }
        }
    }
    return FALSE;
}
#if 0
/* SPF related */
bool_t is_interface_l3_bidirectional(interface_t *interface)
{
    interface_t *neighbour_intf;

    /* The current interface and its neighbour interface should be UP */
    neighbour_intf = &interface->link->intf1 == interface ? &interface->link->intf2 : &interface->link->intf1;
    if(!neighbour_intf)
        return FALSE;

    if(!IF_IS_UP(interface) || !IF_IS_UP(neighbour_intf))
        return FALSE;
    
    /* The current interface and its neighbour interface are not operating in L2 mode */
    if(IF_L2_MODE(interface) == ACCESS || IF_L2_MODE(neighbour_intf) == ACCESS)
        return FALSE;
    
    if(IF_L2_MODE(interface) == TRUNK || IF_L2_MODE(neighbour_intf) == TRUNK)
        return FALSE;

    /* The current interface and its neighbour interface are configured with IP addresses */
    if(!IS_INTF_L3_MODE(interface) || !IS_INTF_L3_MODE(neighbour_intf))
        return FALSE;

    /* The current interface and its neighbour interface should be in the same subnet */
    if(!is_same_subnet(IF_IP(interface), IF_MASK(interface), IF_IP(neighbour_intf)))
        return FALSE;

    if(!is_same_subnet(IF_IP(neighbour_intf), IF_MASK(neighbour_intf), IF_IP(interface)))
        return FALSE;

    return TRUE;
}

bool_t is_same_subnet(char *ip_addr, uint16_t mask, char *ip_to_compare)
{
    uint32_t ip_addr_int;
    uint32_t ip_to_compare_int;
    uint32_t count = 0U;
    uint32_t idx = 31U;

    inet_pton(AF_INET, ip_addr, &ip_addr_int);
    ip_addr_int = htonl(ip_addr_int);
    inet_pton(AF_INET, ip_to_compare, &ip_to_compare_int);
    ip_to_compare_int = htonl(ip_to_compare_int);

    for(idx = 31U; idx >= 0U; idx--){
        if (((ip_addr_int >> idx) & 1U) == ((ip_to_compare_int >> idx) & 1U))
        {
            count++;
        }
    }
    if(count != mask)
        return FALSE;
        
    return TRUE;
}
#endif
#if 1
bool_t
is_interface_l3_bidirectional(interface_t *interface){

    /* If interface is not configured 
     * with IP address*/
    if(!IS_INTF_L3_MODE(interface)){
        printf("%s: Interface is not in L3 mode\n", __FUNCTION__);
        return FALSE;
    }
#if 0
    /*if interface is in L2 mode*/
    if (IF_L2_MODE(interface) == ACCESS || IF_L2_MODE(interface) == TRUNK)
    {
        printf("%s: Interface is either in trunk or access mode\n", __FUNCTION__);
        return FALSE;
    }
#endif        

    interface_t *other_interface = &interface->link->intf1 == interface ?    \
            &interface->link->intf2 : &interface->link->intf1;

    if(!other_interface){
        printf("%s: Neighbour interface doesn't exist\n", __FUNCTION__);
        return FALSE;
    }

    if(!IF_IS_UP(interface) || !IF_IS_UP(other_interface)){
        printf("%s: Interfaces(s) is/are not UP\n", __FUNCTION__);
        return FALSE;
    }
#if 0
    if(IF_L2_MODE(other_interface) == ACCESS || IF_L2_MODE(interface) == TRUNK){
        printf("%s: Neighbour interface is either in trunk or access mode\n", __FUNCTION__);
        return FALSE;
    }
#endif

    if(!IS_INTF_L3_MODE(other_interface)){
        printf("%s: Neighbour interface not in L3 mode\n", __FUNCTION__);
        return FALSE;
    }

    if(!(is_same_subnet(IF_IP(interface), IF_MASK(interface), IF_IP(other_interface)) &&
        is_same_subnet(IF_IP(other_interface), IF_MASK(other_interface), IF_IP(interface)))){
        printf("%s: Link between two different subnets\n", __FUNCTION__);
        return FALSE;
    }
    printf("%s: The link is bidirectional and in L3 mode\n", __FUNCTION__);
    return TRUE;
}
bool_t is_same_subnet(char *ip_addr, char mask, char *other_ip_addr){

    char intf_subnet[16];
    char subnet2[16];

    memset(intf_subnet, 0 , 16);
    memset(subnet2, 0 , 16);

    apply_mask(ip_addr, mask, intf_subnet);
    apply_mask(other_ip_addr, mask, subnet2);

    if(strncmp(intf_subnet, subnet2, 16) == 0){
        return TRUE;
    }
    return FALSE;
}

wheel_timer_t *node_get_timer_instance(node_t *node)
{
    return node->node_nw_prop.wt;
}

#endif

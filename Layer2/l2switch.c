#include "layer2.h"
#include "l2switch.h"
#include "comm.h"
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "tcpconst.h"

GLTHREAD_TO_STRUCT(mac_entry_glue_to_mac_entry, mac_table_entry_t, mac_entry_glue);

static bool_t l2_switch_flood_pkt_out(node_t *node, interface_t *exempted_intf, char *pkt, unsigned int pkt_size);

/* MAC Table APIs */

void init_mac_table(mac_table_t **mac_table)
{
    *mac_table = calloc(1, sizeof(mac_table_t));
    init_glthread(&((*mac_table)->mac_entries)); //??
}

bool_t mac_table_entry_add(mac_table_t *mac_table, mac_table_entry_t *mac_table_entry)
{
    mac_table_entry_t *mac_table_entry_old = mac_table_lookup(mac_table, mac_table_entry->mac.mac_addr);
    if(memcmp(mac_table_entry_old, mac_table_entry, sizeof(mac_table_entry_t)) == 0U){
        return FALSE;
    }
    if(mac_table_entry_old){
        delete_mac_table_entry(mac_table, mac_table_entry->mac.mac_addr);
    }
    init_glthread(&mac_table_entry->mac_entry_glue);
    glthread_add_next(&mac_table->mac_entries, &mac_table_entry->mac_entry_glue); // ?????
    return TRUE;
} 

mac_table_entry_t *mac_table_lookup(mac_table_t *mac_table, char *mac)
{
    mac_table_entry_t *mac_table_entry;
    glthread_t *curr;

    ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries, curr){
        mac_table_entry = mac_entry_glue_to_mac_entry(curr);
        if(strncmp(mac_table_entry->mac.mac_addr, mac, sizeof(mac_add_t)) == 0U){
            return mac_table_entry;
        }
    }ITERATE_GLTHREAD_END(&mac_table->mac_entries, curr);
    return NULL;
}

void clear_mac_table(mac_table_t *mac_table){

    glthread_t *curr;
    mac_table_entry_t *mac_table_entry;

    ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries, curr){
        
        mac_table_entry = mac_entry_glue_to_mac_entry(curr);
        remove_glthread(curr);
        free(mac_table_entry);
    } ITERATE_GLTHREAD_END(&mac_table->mac_entries, curr);
}

void delete_mac_table_entry(mac_table_t *mac_table, char *mac)
{
    mac_table_entry_t *mac_table_entry = mac_table_lookup(mac_table, mac);
    if(!mac_table_entry){
        return;
    }
    remove_glthread(&mac_table_entry->mac_entry_glue);
    free(mac_table_entry);
}

void dump_mac_table(mac_table_t *mac_table)
{
    glthread_t *curr;
    mac_table_entry_t *mac_entry;
    printf("*************MAC TABLE*******************\n");
    ITERATE_GLTHREAD_BEGIN(&mac_table->mac_entries, curr){
        mac_entry = mac_entry_glue_to_mac_entry(curr);
        printf("|Mac: %u:%u:%u:%u:%u:%u | Interface: %s |\n",
                mac_entry->mac.mac_addr[0], mac_entry->mac.mac_addr[1],
                mac_entry->mac.mac_addr[2], mac_entry->mac.mac_addr[3],
                mac_entry->mac.mac_addr[4], mac_entry->mac.mac_addr[5],
                mac_entry->oif_name);
    }ITERATE_GLTHREAD_END(&mac_table->mac_entries, curr);
}

void l2_switch_recv_frame(interface_t *interface, char *pkt, unsigned int pkt_size)
{
    node_t *node = interface->att_node;

    ethernet_frame_t *eth_frame = (ethernet_frame_t *)pkt;

    char *dst_mac = (char *)eth_frame->dst_mac.mac_addr;
    char *src_mac = (char *)eth_frame->src_mac.mac_addr;

    l2_switch_perform_mac_learning(node, src_mac, interface->if_name);
    l2_switch_forward_frame(node, interface, eth_frame, pkt_size);

}

static bool_t l2_switch_send_pkt_out(char *pkt, unsigned int pkt_size, interface_t *oif)
{
    assert(!IS_INTF_L3_MODE(oif));
    intf_l2_mode_t intf_l2_mode = IF_L2_MODE(oif);

    if(IF_L2_MODE(oif) == L2_MODE_UNKNOWN){
        return FALSE;
    }

    vlan_8021q_hdr_t *vlan_hdr = is_pkt_vlan_tagged((ethernet_frame_t *)pkt);

    switch(intf_l2_mode)
    {
        case ACCESS:
        {
            unsigned int intf_vlan_id = get_access_intf_operating_vlan_id(oif);
            /* Case 1: Interface operating in access mode, but not in any vlan, pkt is also untagged
               pkt is simply forwarded - vlan unaware case */
            if(!vlan_hdr && !intf_vlan_id){
                /* some switches assigns default vlan id 1 while ingress
                   some switches drops the pkt 
                   TBR */
                send_pkt_out(pkt, pkt_size, oif);
                return TRUE;
            }

            /* Case 2: If oif is vlan aware, but received a untagged pkt,
               drop the pkt */
            if(!vlan_hdr && intf_vlan_id){
                return FALSE;
            }

            /* Case 3: If oif is vlan aware, receives a tagged pkt, 
               forward only if the vlan id matches
               forward after untagging */
            if(vlan_hdr && intf_vlan_id == GET_802_1Q_VLAN_ID(vlan_hdr)){
                unsigned int new_pkt_size = 0U;
                ethernet_frame_t *eth_pkt = untag_pkt_with_vlan_id((ethernet_frame_t *)pkt, pkt_size, &new_pkt_size);
                send_pkt_out((char *)eth_pkt, new_pkt_size, oif);
                return TRUE;
            }

            /* Case 4: if oif is vlan unaware, but pkt is vlan tagged
               Drop the pkt */
            if(!intf_vlan_id && vlan_hdr){
                return FALSE;
            }
            //break;
        }
        break;

        case TRUNK:
        {
            unsigned int pkt_vlan_id = 0U;

            /* if oif is vlan aware, and pkt is tagged 
               send the pkt only if one of the vlan id matches */
            if(vlan_hdr){
                pkt_vlan_id = GET_802_1Q_VLAN_ID(vlan_hdr);
            }

            if(pkt_vlan_id && is_trunk_intf_vlan_enabled(oif, pkt_vlan_id)){
                send_pkt_out(pkt, pkt_size, oif);
                return TRUE;
            }

            /* Discard pkt otherwise */
            return FALSE;
        }
        break;

        default:
        {
            return FALSE;
        }
        break;
    }
}
#if 0
static bool_t l2_switch_flood_pkt_out(node_t *node, interface_t *exempted_intf, char *pkt, unsigned int pkt_size)
{
    interface_t *oif = NULL;
    unsigned int i = 0U;
    for(; i < MAX_IF_PER_NODE; i++){
        oif = &node->intf[i];
        if((oif != exempted_intf) && (!oif) && !IS_INTF_L3_MODE(oif)){
            send_pkt_out(pkt, pkt_size, oif);
        }
    }
    return TRUE;
}
#endif

void l2_switch_perform_mac_learning(node_t *node, char *src_mac, char *if_name)
{
    bool_t rc;
    /* have to do lookup first - otherwise multiple copies possible */
    mac_table_entry_t *mac_entry = calloc(1, sizeof(mac_table_entry_t));
    memcpy(mac_entry->mac.mac_addr, src_mac, sizeof(mac_add_t));
    strncpy(mac_entry->oif_name, if_name, IF_NAME_SIZE);
    mac_entry->oif_name[IF_NAME_SIZE -1] = '\0';
    rc = mac_table_entry_add(NODE_MAC_TABLE(node), mac_entry);
    if(rc == FALSE)
        free(mac_entry);
}

void l2_switch_forward_frame(node_t *node, interface_t *recv_intf, ethernet_frame_t *eth_frame, unsigned int pkt_size)
{
    /* check if dst mac address is a broadcast address */
    if(IS_MAC_BROADCAST_ADDR(eth_frame->dst_mac.mac_addr)){
        l2_switch_flood_pkt_out(node, recv_intf, (char *)eth_frame, pkt_size);
        return;
    }

    /* Check the mac table to forward the frame */
    mac_table_entry_t *mac_entry = mac_table_lookup(NODE_MAC_TABLE(node), eth_frame->dst_mac.mac_addr);
    if(!mac_entry){
        l2_switch_flood_pkt_out(node, recv_intf, (char *)eth_frame, pkt_size);
        return;
    }
    char *oif_name = mac_entry->oif_name;
    interface_t *oif = get_node_intf_by_name(node, oif_name);
    if(!oif){
        return;
    }
    l2_switch_send_pkt_out((char *)eth_frame, pkt_size, oif);
}

static bool_t l2_switch_flood_pkt_out(node_t *node, interface_t *exempted_intf, char *pkt, unsigned int pkt_size)
{
    interface_t *oif = NULL;
    unsigned int i = 0U;

    char *tmp_buff = calloc(1, MAX_SEND_BUFFER_SIZE);
    memcpy(tmp_buff, pkt, pkt_size);
    for(; i < MAX_IF_PER_NODE; i++)
    {
        oif = node->intf[i];
        if(!oif){
            if((oif == exempted_intf) || (IS_INTF_L3_MODE(oif)))
            {
                continue;
            }
            else
            {
                l2_switch_send_pkt_out(tmp_buff, pkt_size, oif);
            }
        }
    }
    free(tmp_buff);
    return TRUE;
}
#include "layer2.h"
#include "l2switch.h"
#include "comm.h"
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "tcpconst.h"


void node_set_intf_l2_mode(node_t *node, char *intf_name, intf_l2_mode_t intf_l2_mode)
{
    interface_t *intf = get_node_intf_by_name(node, intf_name);
    assert(intf);
    //intf->intf_nw_prop.intf_l2_mode = intf_l2_mode;
    interface_set_l2_mode(node, intf, intf_l2_mode_str(intf_l2_mode));
}

void interface_set_l2_mode(node_t *node, interface_t *interface, char *l2_mode_option)
{
    /* choose the L2 mode based on input parameter */
    intf_l2_mode_t intf_l2_mode;
    if(strncmp(l2_mode_option, "access", strlen("access")) == 0U){
        intf_l2_mode = ACCESS;
    }
    else if(strncmp(l2_mode_option, "trunk", strlen("trunk")) == 0U){
        intf_l2_mode = TRUNK;
    }
    else{
        intf_l2_mode = L2_MODE_UNKNOWN;
        assert(0);
    }

    /* Case 1: If interface was working in L3 mode (i.e an IP address is configured),
    disable the IP address and configured interface to L2 mode */
    if(IS_INTF_L3_MODE(interface)){
        interface->intf_nw_prop.is_ipaddr_config = FALSE;
        interface->intf_nw_prop.is_ipaddr_config_backup = TRUE;
        IF_L2_MODE(interface) = intf_l2_mode;
        return;
    }

    /* Case 2: If interface is so for neither in L2 mode nor L3 mode, then, set l2 mode now */
    if(IF_L2_MODE(interface) == L2_MODE_UNKNOWN){
        IF_L2_MODE(interface) = intf_l2_mode;
        return;
    }

    /* case 3: If interface is already configured as L2 mode, and user requests for the same again, ignore and return */
    if(IF_L2_MODE(interface) == intf_l2_mode){
        return;
    }

    /* case 4: If interface is configured in ACCESS mode, and TRUNK mode is requested, overwrite */
    if(IF_L2_MODE(interface) == ACCESS && intf_l2_mode == TRUNK){
        IF_L2_MODE(interface) = intf_l2_mode;
        /* will have to add necssary vlan ids for trunk mode configuration to complete */
        return;
    }

    /* Case 5: If interface is configured in TRUNK mode, and ACCESS mode is requested, 
       change L2 mode and clean all vlan Ids tagged */
       if(IF_L2_MODE(interface) == TRUNK && intf_l2_mode == ACCESS){
        unsigned int i = 0U;
        for(; i < MAX_VLAN_MEMBERSHIP; i++){
            interface->intf_nw_prop.vlans[i] = 0U;
        }
        IF_L2_MODE(interface) = intf_l2_mode;
        //return;
       }
}

void interface_unset_l2_mode(node_t *node, interface_t *interface, char *l2_mode_option)
{

}

void node_set_intf_vlan_membership(node_t *node, char *intf_name, unsigned int vlan_id)
{
    interface_t *intf = get_node_intf_by_name(node, intf_name);
    assert(intf);
    interface_set_vlan(node, intf, vlan_id);
}

void interface_set_vlan(node_t *node, interface_t *intf, unsigned int vlan_id)
{
    /* Case 1 : Can't tag vlan id for an interface configured with IP address */
    if(IS_INTF_L3_MODE(intf)){
        printf("Error: L3 Mode enabled in interface %s\n", intf->if_name);
        return;
    }

    /* Case 2: Can't set vlan on interface not configured in L2 mode */
    if((IF_L2_MODE(intf) != ACCESS) && (IF_L2_MODE(intf) != TRUNK)){
        printf("Error: L2 mode not enabled in interface %s\n", intf->if_name);
        return;
    }

    /* Case 3: Can set only one vlan on interface operating in ACCESS mode*/
    if(intf->intf_nw_prop.intf_l2_mode == ACCESS){
        unsigned int i = 1U;
        unsigned int count = 0U;
        unsigned int *vlan = NULL;

        for (; i < MAX_VLAN_MEMBERSHIP; i++)
        {
            if (intf->intf_nw_prop.vlans[i])
            {
                intf->intf_nw_prop.vlans[i] = 0U;
                count++;
            }
        }
        if(count)
            printf("Interface %s in access mode configured with more than one vlan Id\n", intf->if_name);
        intf->intf_nw_prop.vlans[i] = vlan_id;
    }

    /* Case 4 : Add vlan membership for interface operating in TRUNK mode */
    if(intf->intf_nw_prop.intf_l2_mode == TRUNK)
    {
        unsigned int i = 0U;
        /* Search for duplicated entries */
        for(; i < MAX_VLAN_MEMBERSHIP; i++)
        {
            if(intf->intf_nw_prop.vlans[i] == vlan_id)
            {
                return;
            }
            else if(intf->intf_nw_prop.vlans[i] == 0U)
            {
                intf->intf_nw_prop.vlans[i] = vlan_id;
                return;
            }
        }
        printf("Error: Max vlan slot per interface of %s exceeded, increase the vlan slot size\n", intf->if_name);
    }
}

void interface_unset_vlan(node_t *node,interface_t *interface, uint32_t vlan)
{
    
}

/* caller needs to free calloc-ed memory after copying to actual buffer */
ethernet_frame_t *tag_pkt_with_vlan_id(ethernet_frame_t *eth_pkt, unsigned int total_pkt_size, \
    int vlan_id, unsigned int *new_pkt_size)
{
    vlan_8021q_hdr_t *vlan_hdr;
    vlan_hdr = is_pkt_vlan_tagged(eth_pkt);
    if(!vlan_hdr){
        vlan_ethernet_frame_t *vlan_eth_pkt = calloc(1, sizeof(vlan_ethernet_frame_t));
        memset(vlan_eth_pkt, 0, sizeof(vlan_ethernet_frame_t));

        /*set up vlan header first */
        vlan_eth_pkt->vlan_8021q_hdr.tpid = 0x8100U;
        vlan_eth_pkt->vlan_8021q_hdr.tci_vid = vlan_id;
        vlan_eth_pkt->vlan_8021q_hdr.tci_dei = 0U;
        vlan_eth_pkt->vlan_8021q_hdr.tci_pcp = 0U;

        /* set up ethernet header */
        memcpy(&vlan_eth_pkt->dst_mac, &eth_pkt->dst_mac, sizeof(mac_add_t));
        memcpy(&vlan_eth_pkt->src_mac, &eth_pkt->src_mac, sizeof(mac_add_t));
        vlan_eth_pkt->type = eth_pkt->type;
        memcpy(&vlan_eth_pkt->payload, eth_pkt->payload, (total_pkt_size - ETH_HDR_SIZE_EXCL_PAYLOAD));
        /* Actually FCS should be re-calculated - we ignore it for now */
        *new_pkt_size = total_pkt_size + sizeof(vlan_8021q_hdr_t);
        return (ethernet_frame_t *)&vlan_eth_pkt;
    }
    else{
        vlan_hdr->tci_vid = vlan_id;
        *new_pkt_size = total_pkt_size;
        return eth_pkt;
    }
    return NULL;
}

ethernet_frame_t *untag_pkt_with_vlan_id(ethernet_frame_t *eth_pkt, unsigned int total_pkt_size, unsigned int *new_pkt_size)
{
    vlan_8021q_hdr_t *vlan_hdr;
    vlan_hdr = is_pkt_vlan_tagged(eth_pkt);
    if(!vlan_hdr){
        return eth_pkt;
    }
    else{
        *new_pkt_size = total_pkt_size - sizeof(vlan_8021q_hdr_t);
        ethernet_frame_t *ethernet_pkt = calloc(1, *new_pkt_size );
        memset(ethernet_pkt, 0, *new_pkt_size);
        memcpy(&ethernet_pkt->dst_mac, &eth_pkt->dst_mac, sizeof(mac_add_t));
        memcpy(&ethernet_pkt->src_mac, &eth_pkt->src_mac, sizeof(mac_add_t));
        memcpy(&ethernet_pkt->payload, &eth_pkt->payload, (*new_pkt_size - VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD));
        ethernet_pkt->type = eth_pkt->type;
        /* FCS should be calculated a-fresh */
        return ethernet_pkt;
    }
    return NULL;
}

static void pending_arp_processing_callback_function(node_t *node, interface_t *oif, arp_entry_t *arp_entry, arp_pending_entry_t *arp_pending_entry)
{
    ethernet_frame_t *eth_pkt = (ethernet_frame_t *)arp_pending_entry->pkt;
    unsigned int pkt_size = arp_pending_entry->pkt_size;
    memcpy(eth_pkt->dst_mac.mac_addr, arp_entry->mac_addr.mac_addr, sizeof(mac_add_t));
    memcpy(eth_pkt->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
    SET_COMMON_ETH_FCS(eth_pkt, pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(eth_pkt), 0);
    send_pkt_out((char *)eth_pkt, pkt_size, oif);
    printf("%s: Info - pkt size is %u\n", __FUNCTION__, pkt_size);  
}

extern bool_t
is_layer3_local_delivery(node_t *node, uint32_t dst_ip);

static void
l2_forward_ip_packet(node_t *node, unsigned int next_hop_ip, char *outgoing_intf, ethernet_frame_t *pkt, unsigned int pkt_size){
    
    interface_t *oif = NULL;
    char next_hop_ip_str[16];
    arp_entry_t *arp_entry = NULL;
    ethernet_frame_t *eth_pkt = (ethernet_frame_t *)pkt;
    unsigned int ethernet_payload_size = pkt_size - ETH_HDR_SIZE_EXCL_PAYLOAD;

    next_hop_ip = htonl(next_hop_ip);
    inet_ntop(AF_INET, &next_hop_ip, next_hop_ip_str, 16U);
    /* Restore again, since htonl reverses the byte order */
    //next_hop_ip = htonl(next_hop_ip); // This is not required and unnecessarily reverses the byte order

    if(outgoing_intf){
        /* Case #1 : l2 forwarding 
           It means, L3 has resolved the next hop IP address,
           L2 has to forward the Pkt out of this interface.
        */
       oif = get_node_intf_by_name(node, outgoing_intf);
       printf("%s: Info - outgoing interface is is %s\n", __FUNCTION__, oif->if_name);
       assert(oif);
       arp_entry = arp_table_lookup(NODE_ARP_TABLE(node), next_hop_ip_str);

       if(!arp_entry){
            /* Time for ARP resolution */
            printf("%s: Info - ARP entry doesn't exist. create a sane entry\n", __FUNCTION__);
            arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node), next_hop_ip_str);
            printf("%s: Info - Sane ARP entry created @ %x\n", __FUNCTION__, arp_entry);
            add_arp_pending_entry(arp_entry, pending_arp_processing_callback_function, (char *)pkt, pkt_size);
            send_arp_broadcast_request(node, oif, next_hop_ip_str);
            return;
       }
       else if (arp_entry_sane(arp_entry)){
            add_arp_pending_entry(arp_entry, pending_arp_processing_callback_function, (char *)pkt, pkt_size);
            printf("%s: Info - If sane entry is available, configure a callback when arp entry becomes full\n", __FUNCTION__);
            return;
       }
       else 
            goto l2_frame_prepare;
    }

    /* Case #4: Self ping */
    if(is_layer3_local_delivery(node, next_hop_ip)){
        promote_pkt_to_layer3(node, NULL, GET_ETHERNET_HDR_PAYLOAD(eth_pkt), pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(eth_pkt), eth_pkt->type);
        return;
    }

    /* Case #2: Direct host delivery 
       L2 has to forward the frame to machine on local connected subnet */
    oif = node_get_matching_subnet_interface(node, next_hop_ip_str);
    if(!oif){
        printf("%s: Error - Local machine subnet for IP %s could not be found in node %s\n", __FUNCTION__, next_hop_ip_str, node->node_name);
        return;
    }
    arp_entry = arp_table_lookup(NODE_ARP_TABLE(node), next_hop_ip_str);
    if(!arp_entry){
        /* Time for ARP resolution */
        arp_entry = create_arp_sane_entry(NODE_ARP_TABLE(node), next_hop_ip_str);
        add_arp_pending_entry(arp_entry, pending_arp_processing_callback_function, (char *)pkt, pkt_size);
        send_arp_broadcast_request(node, oif, next_hop_ip_str);
        return;
    }
    else if (arp_entry_sane(arp_entry)){
        add_arp_pending_entry(arp_entry, pending_arp_processing_callback_function, (char *)pkt, pkt_size);
        return;
    }

    l2_frame_prepare:
        memcpy(eth_pkt->dst_mac.mac_addr, arp_entry->mac_addr.mac_addr, sizeof(mac_add_t));
        memcpy(eth_pkt->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
        SET_COMMON_ETH_FCS(eth_pkt, ethernet_payload_size, 0);
        send_pkt_out((char *)eth_pkt, pkt_size, oif);
        printf("%s: Info - Sending the ICMP out through interface %s\n", __FUNCTION__, oif->if_name);
        printf("%s: Info - pkt size is %u\n", __FUNCTION__, pkt_size);
}

extern void l2_switch_recv_frame(interface_t *interface, char *pkt, unsigned int pkt_size);

void layer2_frame_recv(node_t *node, interface_t *interface, char *pkt, unsigned int pkt_size)
{
    unsigned int vlan_id_to_tag = 0U;
    /* Entry in to TCP IP stack from MAC Layer */
    if(l2_frame_recv_qualify_on_interface(interface, (ethernet_frame_t *)pkt, &vlan_id_to_tag) == FALSE){
        printf("L2 frame rejected on node %s\n", node->node_name);
        return;
    }
    printf("L2 frame accepted on node %s\n", node->node_name);

    /* handle reception of a L2 frame on an L3 interface */
    if(IS_INTF_L3_MODE(interface)){
        promote_pkt_to_layer2(node, interface, (ethernet_frame_t *)pkt, pkt_size);
    }
    else if(IF_L2_MODE(interface) == TRUNK || IF_L2_MODE(interface) == ACCESS){
        unsigned int new_pkt_size = 0U;

        if(vlan_id_to_tag){
            pkt = (char *)tag_pkt_with_vlan_id((ethernet_frame_t *)pkt, pkt_size, vlan_id_to_tag, &new_pkt_size);
            assert(new_pkt_size != pkt_size);
        }
        l2_switch_recv_frame(interface, pkt, vlan_id_to_tag? new_pkt_size : pkt_size);
    }
    else    
        return; /* Do nothing; drop the pkt */
}

extern void promote_pkt_to_layer3(node_t *node, interface_t *interface, char *pkt, unsigned int pkt_size, int l3_protocol_type);

static void promote_pkt_to_layer2(node_t *node, interface_t *iif, ethernet_frame_t *eth_frame, unsigned int pkt_size)
{
    printf("%s: Info - pkt size is %u\n", __FUNCTION__, pkt_size);
    switch(eth_frame->type)
    {
        case ARP_MSG:
        {
            arp_packet_t * arp_pkt = (arp_packet_t *)&eth_frame->payload;
            switch(arp_pkt->op_code)
            {
                case ARP_BROAD_REQ:
                {
                    process_arp_broadcast_request(node, iif, eth_frame);
                    break;
                }
                case ARP_REPLY:
                {
                    process_arp_reply_msg(node, iif, eth_frame);
                    break;
                }
                default:
                    break;
            }
        }
        case ETH_IP:
        {
            promote_pkt_to_layer3(node, iif, GET_ETHERNET_HDR_PAYLOAD(eth_frame), pkt_size - GET_ETH_HDR_SIZE_EXCL_PAYLOAD(eth_frame),\
                                  eth_frame->type);
            break;
        }
        default:
        ;
    }
}

static void layer2_pkt_receive_from_top(node_t *node, unsigned int next_hop_ip, char *outgoing_intf, char *pkt, unsigned int pkt_size, int protocol_number)
{
    printf("%s: Info - Pkt size is %u\n", __FUNCTION__, pkt_size);
    printf("%s: Info - Allowed pkt size is %d\n", __FUNCTION__, sizeof(((ethernet_frame_t *)0)->payload));
    assert(pkt_size < sizeof(((ethernet_frame_t *)0)->payload));
    if(protocol_number == ETH_IP){
        ethernet_frame_t * empty_eth_pkt = ALLOC_ETH_HRD_WITH_PAYLOAD(pkt, pkt_size);
        empty_eth_pkt->type = ETH_IP;
        printf("%s: Info - Pkt received in layer 2\n", __FUNCTION__);
        l2_forward_ip_packet(node, next_hop_ip, outgoing_intf, empty_eth_pkt, pkt_size + ETH_HDR_SIZE_EXCL_PAYLOAD);
    }
}

void demote_pkt_to_layer2(node_t *node, unsigned int next_hop_ip, char *outgoing_intf, char *pkt, unsigned int pkt_size, int protocol_number)
{
    layer2_pkt_receive_from_top(node, next_hop_ip, outgoing_intf, pkt, pkt_size, protocol_number);
}

void init_arp_table(arp_table_t **arp_table)
{
    *arp_table = calloc(0, sizeof(arp_table_t));
    init_glthread(&((*arp_table)->arp_entries));
}
/* CRUD ops for ARP table */
bool_t arp_table_entry_add(arp_table_t *arp_table, arp_entry_t *arp_entry, glthread_t **arp_pending_list)
{
    if(arp_pending_list){
        assert(*arp_pending_list == NULL);
    }
    arp_entry_t *arp_entry_old = arp_table_lookup(arp_table, arp_entry->ip_addr.ip_addr);
    /* Case #0: If ARP table does not exist already, then add it and return TRUE */
    if(!arp_entry_old){
        glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue);
        return TRUE;
    }
    /* Case #1: If there is a match with the arp entry in arp table, do nothing */
    if(arp_entry_old && memcmp(arp_entry_old, arp_entry, sizeof(arp_entry_t)) == 0U){
        return FALSE;
    }
    /* Case #2: If there is an old arp entry, replace it */
    if(arp_entry_old && !arp_entry_sane(arp_entry_old))
    {
        delete_arp_entry(arp_entry_old);
        init_glthread(&arp_entry->arp_glue);
        glthread_add_next(&arp_table->arp_entries, &arp_entry->arp_glue);
        return TRUE;
    }

    /*  Case #3: If existing ARP table entry is sane, the new one is also sane.
        Then, move the pending arp list from new to old and one and return FALSE */
    if(arp_entry_old && arp_entry_sane(arp_entry_old) && arp_entry_sane(arp_entry))
    {
        if(!IS_GLTHREAD_LIST_EMPTY(&arp_entry->arp_pending_list)){
            glthread_add_next(&arp_entry_old->arp_pending_list, arp_entry->arp_pending_list.right);
        }
        if(arp_pending_list){
            *arp_pending_list = &arp_entry_old->arp_pending_list;
            return FALSE;
        }
    }
    /*  Case #4: If existing ARP table entry is sane, new one is full, then,
        copy contents of new */
    if(arp_entry_old && arp_entry_sane(arp_entry_old) && !arp_entry_sane(arp_entry)){
        strncpy(arp_entry_old->mac_addr.mac_addr, arp_entry->mac_addr.mac_addr, sizeof(mac_add_t));
        strncpy(arp_entry_old->oif_name, arp_entry->oif_name, IF_NAME_SIZE);
        arp_entry_old->oif_name[IF_NAME_SIZE - 1] = '\0';

        if(arp_pending_list){
            *arp_pending_list = &arp_entry_old->arp_pending_list;
        }
        return FALSE;
    }
    return FALSE;
}

arp_entry_t *arp_table_lookup(arp_table_t *arp_table, char *ip_addr)
{
    glthread_t *curr;
    arp_entry_t *arp_entry;

    ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr){
        arp_entry = arp_glue_to_arp_entry(curr);
        if(strncmp(arp_entry->ip_addr.ip_addr, ip_addr, 16) == 0U)
            return arp_entry;
    }ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr);
    return NULL;
}

static void process_arp_pending_entry(node_t *node, interface_t *oif, arp_entry_t *arp_entry, arp_pending_entry_t *arp_pending_entry){

    arp_pending_entry->cb(node, oif, arp_entry, arp_pending_entry);  
}

static void delete_arp_pending_entry(arp_pending_entry_t *arp_pending_entry){

    remove_glthread(&arp_pending_entry->arp_pending_entry_glue);
    free(arp_pending_entry);
}

void arp_table_update_from_arp_reply(arp_table_t *arp_table, arp_packet_t *arp_pkt, interface_t *iif)
{
    unsigned int src_ip = 0U;
    glthread_t *arp_pending_list = NULL;
    assert(arp_pkt->op_code == ARP_REPLY);
    arp_entry_t *arp_entry = calloc(1, sizeof(arp_entry_t));
    src_ip = htonl(arp_pkt->src_ip);
    inet_ntop(AF_INET, &src_ip, &arp_entry->ip_addr.ip_addr, 16);
    /* To be on safer side */
    arp_entry->ip_addr.ip_addr[15] = '\0';
    memcpy(arp_entry->mac_addr.mac_addr, arp_pkt->src_mac.mac_addr, sizeof(mac_add_t));
    strncpy(arp_entry->oif_name, iif->if_name, IF_NAME_SIZE);
    arp_entry->is_sane = FALSE;

    bool_t rc = arp_table_entry_add(arp_table, arp_entry, &arp_pending_list);
    glthread_t *curr;
    arp_pending_entry_t *arp_pending_entry;

    if(arp_pending_list){
        ITERATE_GLTHREAD_BEGIN(arp_pending_list, curr){
            arp_pending_entry = arp_pending_entry_glue_to_arp_pending_entry_list(curr);
            remove_glthread(&arp_pending_entry->arp_pending_entry_glue);
            process_arp_pending_entry(iif->att_node, iif, arp_entry, arp_pending_entry);
            delete_arp_pending_entry(arp_pending_entry);
        }ITERATE_GLTHREAD_END(arp_pending_list, curr);
        (arp_pending_list_to_arp_entry(arp_pending_list))->is_sane = FALSE;
    }

    if(rc == FALSE){
        delete_arp_entry(arp_entry);
    }
}

void delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr)
{
    arp_entry_t *arp_entry = arp_table_lookup(arp_table, ip_addr);
    if(!arp_entry)
        return;
    delete_arp_entry(arp_entry);
}

void delete_arp_entry(arp_entry_t *arp_entry)
{
    glthread_t *curr;
    arp_pending_entry_t *arp_pending_entry;
    remove_glthread(&arp_entry->arp_glue);
    ITERATE_GLTHREAD_BEGIN(&arp_entry->arp_pending_list, curr){
        arp_pending_entry = arp_pending_entry_glue_to_arp_pending_entry_list(curr);
        delete_arp_pending_entry(arp_pending_entry);
    }ITERATE_GLTHREAD_END(&arp_entry->arp_pending_list, curr);
    free(arp_entry);
}

void add_arp_pending_entry(arp_entry_t *arp_entry, arp_processing_fn cb, char *pkt, unsigned int pkt_size)
{
    arp_pending_entry_t *arp_pending_entry = calloc(1, sizeof(arp_pending_entry_t) + pkt_size);
    init_glthread(&arp_pending_entry->arp_pending_entry_glue);
    arp_pending_entry->cb = cb;
    arp_pending_entry->pkt_size = pkt_size;
    memcpy(arp_pending_entry->pkt, pkt, pkt_size);
    glthread_add_next(&arp_entry->arp_pending_list, &arp_pending_entry->arp_pending_entry_glue);
}

arp_entry_t *create_arp_sane_entry(arp_table_t *arp_table, char *ip_addr)
{
    /*  If arp table is full, asser. 
        L2 must not create an ARP entry if the entry was already existing */
    arp_entry_t *arp_entry = arp_table_lookup(arp_table, ip_addr);

    if(arp_entry){
        if(!arp_entry_sane(arp_entry)){
            /* No need to create ARP sane entry when ARP complete entry is already available */
            assert(0);
        }
        printf("%s: Info - Existing ARP sane entry found\n",__FUNCTION__);
        return arp_entry;
    }
    /* If ARP entry does not exist, create a new sane entry */
    arp_entry = calloc(1, sizeof(arp_entry_t));
    strncpy(arp_entry->ip_addr.ip_addr, ip_addr, 16U);
    arp_entry->ip_addr.ip_addr[15] = '\0';
    init_glthread(&arp_entry->arp_pending_list);
    arp_entry->is_sane = TRUE;
    bool_t rc = arp_table_entry_add(arp_table, arp_entry, 0);
    if(rc == FALSE)
        assert(0);
    return arp_entry;
}

void dump_arp_table(arp_table_t *arp_table)
{
    glthread_t *curr;
    arp_entry_t *arp_entry;
    printf("*********ARP TABLE**********\n");
    ITERATE_GLTHREAD_BEGIN(&arp_table->arp_entries, curr){
        arp_entry = arp_glue_to_arp_entry(curr);
        printf("IP: %s, MAC: %u:%u:%u:%u:%u:%u, OIF: %s\n",
            arp_entry->ip_addr.ip_addr,
            arp_entry->mac_addr.mac_addr[0],
            arp_entry->mac_addr.mac_addr[1],
            arp_entry->mac_addr.mac_addr[2],
            arp_entry->mac_addr.mac_addr[3],
            arp_entry->mac_addr.mac_addr[4],
            arp_entry->mac_addr.mac_addr[5],
            arp_entry->oif_name);
    }ITERATE_GLTHREAD_END(&arp_table->arp_entries, curr);
}

void send_arp_broadcast_request(node_t *node, interface_t *oif, char * ip_addr)
{
    //arp_packet_t *arp_pkt;
    unsigned int payload_size = sizeof(arp_packet_t);
    ethernet_frame_t *eth_frame = (ethernet_frame_t *)calloc(1, ETH_HDR_SIZE_EXCL_PAYLOAD + payload_size);

    /* ARP resolution is meant to be with another subnet. 
       Find the matching interface for the dst IP address, if oif is not specified */
    if(!oif){
        oif = node_get_matching_subnet_interface(node, ip_addr);
        if(!oif){
            printf("Error: No matching interface for IP address %s found in node %s\n", ip_addr, node->node_name);
            return;
        }
        if(strncmp(IF_IP(oif), ip_addr, 16) == 0U){
            printf("Error: IP address %s is local within the same subnet\n", ip_addr);
            return;
        }
    }
    /* #1 Prepare ethernet header */
    layer2_fill_with_broadcast_mac(eth_frame->dst_mac.mac_addr);
    memcpy(eth_frame->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
    eth_frame->type = ARP_MSG;

    /* #2 Prepare ARP packet */
    /* Add ARP packet as payload to ethernet frame */
    //arp_packet_t *arp_pkt = (arp_packet_t *)&eth_frame->payload;
    arp_packet_t *arp_pkt = (arp_packet_t *)GET_ETHERNET_HDR_PAYLOAD(eth_frame);
    memcpy(arp_pkt->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
    memset(arp_pkt->dst_mac.mac_addr, 0, sizeof(mac_add_t));
    arp_pkt->hw_addr_len = sizeof(mac_add_t);
    arp_pkt->proto_addr_len = 4U;
    arp_pkt->hw_type = 1U;
    arp_pkt->proto_type = 0x0800U;
    arp_pkt->op_code = ARP_BROAD_REQ;

    inet_pton(AF_INET, IF_IP(oif), &arp_pkt->src_ip);
    arp_pkt->src_ip = htonl(arp_pkt->src_ip);
    inet_pton(AF_INET, ip_addr, &arp_pkt->dst_ip);
    arp_pkt->dst_ip = htonl(arp_pkt->dst_ip);

    /* #3 append the ethernet footer */
    SET_COMMON_ETH_FCS(eth_frame, sizeof(arp_packet_t), 0U); /* 0 because, it is NOT used for now */

    dump_eth_frame(eth_frame, "Sent Message");
    /* #4  send  out the ethernet frame */
    send_pkt_out(eth_frame, (ETH_HDR_SIZE_EXCL_PAYLOAD + payload_size), oif);

    printf("Info: Send out the ARP request frame\n");
    free(eth_frame);
}

static void send_arp_reply_msg(ethernet_frame_t *eth_frame_in, interface_t *oif)
{
    arp_packet_t *arp_pkt_in = (arp_packet_t *)&eth_frame_in->payload;

    printf("%s: Preparing ARP reply\n", __FUNCTION__);
    ethernet_frame_t *eth_frame_reply = calloc(1, MAX_SEND_BUFFER_SIZE);
    memcpy(eth_frame_reply->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
    memcpy(eth_frame_reply->dst_mac.mac_addr, arp_pkt_in->src_mac.mac_addr, sizeof(mac_add_t));
    eth_frame_reply->type = ARP_MSG;

    arp_packet_t *arp_pkt_reply = (arp_packet_t *)&eth_frame_reply->payload;
    arp_pkt_reply->hw_type = 1U;
    arp_pkt_reply->hw_addr_len = sizeof(mac_add_t);
    arp_pkt_reply->proto_type = 0x0800;
    arp_pkt_reply->proto_addr_len = 4U;
    arp_pkt_reply->op_code = ARP_REPLY;
    memcpy(arp_pkt_reply->src_mac.mac_addr, IF_MAC(oif), sizeof(mac_add_t));
    memcpy(arp_pkt_reply->dst_mac.mac_addr, arp_pkt_in->src_mac.mac_addr, sizeof(mac_add_t));

    inet_pton(AF_INET, IF_IP(oif), &arp_pkt_reply->src_ip);
    arp_pkt_reply->src_ip = htonl(arp_pkt_reply->src_ip);

    arp_pkt_reply->dst_ip =  arp_pkt_in->src_ip;

    SET_COMMON_ETH_FCS(eth_frame_reply, (ETH_HDR_SIZE_EXCL_PAYLOAD_FCS + sizeof(arp_packet_t)), 0);

    unsigned int total_pkt_size = ETH_HDR_SIZE_EXCL_PAYLOAD + sizeof(arp_packet_t);
    char * shifted_pkt_buffer = pkt_buffer_shift_right((char *)eth_frame_reply, total_pkt_size, MAX_SEND_BUFFER_SIZE);
    printf("%s: Sending ARP reply\n", __FUNCTION__);
    send_pkt_out(shifted_pkt_buffer, total_pkt_size, oif);
    free(eth_frame_reply);
} 

static void process_arp_reply_msg(node_t *node, interface_t *iif, ethernet_frame_t *eth_frame)
{
    printf("ARP reply message received on interface %s of node %s\n", iif->if_name, node->node_name);
    arp_table_update_from_arp_reply(NODE_ARP_TABLE(node), (arp_packet_t *)GET_ETHERNET_HDR_PAYLOAD(eth_frame), iif);
}

static void process_arp_broadcast_request(node_t *node, interface_t *iif, ethernet_frame_t *eth_frame)
{
    printf("%s : ARP broadcast msg received on interface %s of node %s\n", __FUNCTION__, iif->if_name, node->node_name);

    char ip_addr[16];
    dump_eth_frame(eth_frame, "Received Message");
    arp_packet_t *arp_pkt = (arp_packet_t *)(GET_ETHERNET_HDR_PAYLOAD(eth_frame)); //pkt corrupted
    memset(ip_addr, '\0', 16);
    unsigned int arp_dst_ip = htonl(arp_pkt->dst_ip);
    inet_ntop(AF_INET, &arp_dst_ip, ip_addr, 16U);//??
    ip_addr[15] = '\0';
    if(strncmp(IF_IP(iif), ip_addr, 16))
    {
        printf("%s : ARP request msg dropped at node %s as the destination IP %s doesn't match\
               with the interface IP %s\n", __FUNCTION__, node->node_name, ip_addr, IF_IP(iif));
        return;
    }
    send_arp_reply_msg(eth_frame, iif);
}

void dump_eth_frame(ethernet_frame_t *eth_pkt, char *msg)
{
    printf("%s\n",msg);
    printf("Destination MAC : %x:%x:%x:%x:%x:%x\n", eth_pkt->dst_mac.mac_addr[0],  eth_pkt->dst_mac.mac_addr[1], \
                                                    eth_pkt->dst_mac.mac_addr[2],  eth_pkt->dst_mac.mac_addr[3], \
                                                    eth_pkt->dst_mac.mac_addr[4],  eth_pkt->dst_mac.mac_addr[5]);
    printf("Source MAC      : %x:%x:%x:%x:%x:%x\n", eth_pkt->src_mac.mac_addr[0],  eth_pkt->src_mac.mac_addr[1], \
                                               eth_pkt->src_mac.mac_addr[2],  eth_pkt->src_mac.mac_addr[3], \
                                               eth_pkt->src_mac.mac_addr[4],  eth_pkt->src_mac.mac_addr[5]);  
    printf("Frame type      : %d\n",eth_pkt->type);  
    
    if(eth_pkt->type == ARP_MSG)
    {
        arp_packet_t *arp_pkt = (arp_packet_t *)(GET_ETHERNET_HDR_PAYLOAD(eth_pkt));
        char ip_addr[16];
        printf("\tHwType               : %d\n", arp_pkt->hw_type); // 1 for ethernet cable
        printf("\tProtoType            : %d\n", arp_pkt->proto_type); // 0x0800 for IPv4
        printf("\tHw Address Length    : %d\n", arp_pkt->hw_addr_len); // 6 for MAC
        printf("\tProto Address Length : %d\n", arp_pkt->proto_addr_len); // 4 for IPv4
        printf("\tARP Request/Response : %d\n", arp_pkt->op_code); //1 for request; 2 for response
        printf("\tSource MAC           : %x:%x:%x:%x:%x:%x\n", arp_pkt->src_mac.mac_addr[0],  arp_pkt->src_mac.mac_addr[1], \
            arp_pkt->src_mac.mac_addr[2],  arp_pkt->src_mac.mac_addr[3], \
            arp_pkt->src_mac.mac_addr[4],  arp_pkt->src_mac.mac_addr[5]);  
        ip_addr_n_to_p(arp_pkt->src_ip, ip_addr);   
        printf("\tSource IP            : %s\n", ip_addr);
        printf("\tDestination MAC      : %x:%x:%x:%x:%x:%x\n", arp_pkt->dst_mac.mac_addr[0],  arp_pkt->dst_mac.mac_addr[1], \
            arp_pkt->dst_mac.mac_addr[2],  arp_pkt->dst_mac.mac_addr[3], \
            arp_pkt->dst_mac.mac_addr[4],  arp_pkt->dst_mac.mac_addr[5]);
        memset(ip_addr, '\0', 16U);
        ip_addr_n_to_p(arp_pkt->dst_ip, ip_addr);  
        printf("\tDestination IP       : %s\n", ip_addr);
    }
    printf("Ethernet FCS/CRC: %x\n", eth_pkt->FCS);
}
#ifndef LAYER_2_H
#define LAYER_2_H

#include "../net.h"
#include "../graph.h"
#include "../utils.h"
#include "layer2.h"
#include "../comm.h"
#include "../glueThread/glthread.h"
#include <stdio.h>

#pragma pack(push, 1)
typedef struct arp_packet_
{
    short hw_type;                  /* 1 for ethernet cable */
    short proto_type;               /* 0x0800 for IPv4      */
    char hw_addr_len;               /* 6 for MAC */
    char proto_addr_len;            /* 4 for IPv4*/
    short op_code;                  /* 1 for request; 2 for reply */
    mac_add_t src_mac;              /* MAC of OIF interface */
    unsigned int src_ip;            /* IP of OIF interface */
    mac_add_t dst_mac;              /* ? would be a broadcast for first time; if a gateway exists, we use it next time onwards */
    unsigned int dst_ip;            /* Ip for which ARP resolution is required */
}arp_packet_t;

/* IEEE 802.3 ethernet frame */
typedef struct ethernet_frame_
{
    mac_add_t dst_mac;
    mac_add_t src_mac;
    unsigned short type; /* 2 Bytes */
    char payload[248];   /* max is 1500 Bytes */
    unsigned int FCS;

} ethernet_frame_t;

/* 802.1Q vlan header */
typedef struct vlan_8021q_hdr_
{
    unsigned short tpid;   /* 0x8100 */
    short tci_pcp : 3;
    short tci_dei : 1;
    short tci_vid : 12;   /* Tagged VLAN Id */
}vlan_8021q_hdr_t;

/* IEEE 802.3 ethernet frame with Vlan header */
typedef struct vlan_ethernet_frame_
{
    mac_add_t dst_mac;
    mac_add_t src_mac;
    vlan_8021q_hdr_t vlan_8021q_hdr;
    unsigned short type;
    char payload[248]; /* range is 46 - 1500 Bytes */
    unsigned int FCS;
}vlan_ethernet_frame_t;
#pragma pop

typedef struct arp_pending_entry_ arp_pending_entry_t;
typedef struct arp_entry_ arp_entry_t;
typedef void (*arp_processing_fn)(node_t * node, interface_t *oif, arp_entry_t *arp_entry, arp_pending_entry_t *arp_pending_entry);

/* ARP table APIs */
typedef struct arp_table_{
    glthread_t arp_entries;
}arp_table_t;

struct arp_pending_entry_{
    glthread_t arp_pending_entry_glue;
    arp_processing_fn cb;
    unsigned int pkt_size; /* size including ethernet header */
    char pkt[0];
};

typedef struct arp_entry_{
    ip_add_t  ip_addr;
    mac_add_t mac_addr;
    char oif_name[IF_NAME_SIZE];
    glthread_t arp_glue;
    bool_t is_sane;
    /* List of packets which are pending for ARP resolution */
    glthread_t arp_pending_list;
    wheel_timer_elem_t *exp_timer_wt_elem;
}arp_entry_t;

GLTHREAD_TO_STRUCT(arp_glue_to_arp_entry, arp_entry_t, arp_glue);
GLTHREAD_TO_STRUCT(arp_pending_entry_glue_to_arp_pending_entry_list, arp_pending_entry_t, arp_pending_entry_glue);
GLTHREAD_TO_STRUCT(arp_pending_list_to_arp_entry, arp_entry_t, arp_pending_list);


#define arp_glue_to_arp_entry(glthread_ptr) \
    (arp_entry_t *)((char *)glthread_ptr - (char *)&(((arp_entry_t *)0)->arp_glue))

#define ETH_HDR_SIZE_EXCL_PAYLOAD \
    (sizeof(ethernet_frame_t) - sizeof(((ethernet_frame_t *)0)->payload))

#define ETH_HDR_SIZE_EXCL_PAYLOAD_FCS \
    (sizeof(ethernet_frame_t) - sizeof(((ethernet_frame_t *)0)->payload) - sizeof(((ethernet_frame_t *)0)->FCS))

#define ETH_FCS(eth_frame_ptr, payload_size) \
    (*(unsigned int *)(((char *)(((ethernet_frame_t *)eth_frame_ptr)->payload) + payload_size)))

#define VLAN_ETH_FCS(vlan_eth_frame_ptr, payload_size) \
    (*(unsigned int *)(((char *)(((vlan_ethernet_frame_t *)vlan_eth_frame_ptr)->payload) + payload_size)))

#define VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD \
    (sizeof(vlan_ethernet_frame_t) - sizeof(((vlan_ethernet_frame_t *)0)->payload))

#define VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD_FCS \
    (sizeof(vlan_ethernet_frame_t) - sizeof(((vlan_ethernet_frame_t *)0)->payload) - sizeof(((ethernet_frame_t *)0)->FCS))

#define GET_COMMON_ETH_FCS(eth_frame_ptr,payload_size) \
        if(eth_frame_ptr->type){return (*(vlan_ethernet_frame_t *)&eth_frame_ptr->FCS);}  \
        else{return eth_frame_ptr->FCS;} 

//GLTHREAD_TO_STRUCT(arp_glue_to_arp_entry, arp_entry_t, arp_glue);
//GLTHREAD_TO_STRUCT(arp_pending_list_to_arp_entry, arp_entry_t, arp_pending_list);
        
void node_set_intf_l2_mode(node_t *node, char *intf_name, intf_l2_mode_t intf_l2_mode);
void interface_set_l2_mode(node_t *node, interface_t *interface, char *l2_mode_option);
void node_set_intf_vlan_membership(node_t *node, char *intf_name, unsigned int vlan_id);
void interface_set_vlan(node_t *node, interface_t *intf, unsigned int vlan_id);
ethernet_frame_t *tag_pkt_with_vlan_id(ethernet_frame_t *eth_pkt, unsigned int total_pkt_size, int vlan_id, unsigned int *new_pkt_size);
ethernet_frame_t *untag_pkt_with_vlan_id(ethernet_frame_t *eth_pkt, unsigned int total_pkt_size, unsigned int *new_pkt_size);
void layer2_frame_recv(node_t *node, interface_t *interface, char *pkt, unsigned int pkt_size);
static void promote_pkt_to_layer2(node_t *node, interface_t *interface, ethernet_frame_t *eth_frame, unsigned int pkt_size);
//void init_arp_table(arp_table_t **arp_table);
bool_t arp_table_entry_add(node_t *node, arp_table_t *arp_table, arp_entry_t *arp_entry, glthread_t **arp_pending_list);
arp_entry_t *arp_table_lookup(arp_table_t *arp_table, char *ip_addr);
void arp_table_update_from_arp_reply(arp_table_t *arp_table, arp_packet_t *arp_pkt, interface_t *iif);
void delete_arp_table_entry(arp_table_t *arp_table, char *ip_addr);
void dump_arp_table(arp_table_t *arp_table);
void delete_arp_entry(arp_entry_t *arp_entry);
void send_arp_broadcast_request(node_t *node, interface_t *oif, char * ip_addr);
void dump_eth_frame(ethernet_frame_t *eth_pkt, char *msg);

static void send_arp_reply_msg(ethernet_frame_t *eth_frame_in, interface_t *oif);
static void process_arp_reply_msg(node_t *node, interface_t *iif, ethernet_frame_t *eth_frame);
static void process_arp_broadcast_request(node_t *node, interface_t *iif, ethernet_frame_t *eth_frame);

wheel_timer_elem_t *arp_entry_create_expiration_timer(node_t *node, arp_entry_t *arp_entry, uint16_t exp_time);
void arp_entry_delete_expiration_timer(arp_entry_t *arp_entry);
void arp_entry_refresh_expiration_timer(arp_entry_t *arp_entry);
uint16_t arp_entry_get_exp_time_left(arp_entry_t *arp_entry);

//arp_entry_t *create_arp_sane_entry(arp_table_t *arp_table, char *ip_addr);
void create_arp_sane_entry(node_t *node, arp_table_t *arp_table, char *ip_addr, char *pkt, uint32_t pkt_size);
static bool_t arp_entry_sane(arp_entry_t *arp_entry){
    return arp_entry->is_sane;
}



static inline ethernet_frame_t *ALLOC_ETH_HRD_WITH_PAYLOAD(char *pkt, unsigned int pkt_size);

static inline bool_t l2_frame_recv_qualify_on_interface(interface_t *intf, ethernet_frame_t *frame, unsigned int *output_vlan_id);

static inline void SET_COMMON_ETH_FCS(ethernet_frame_t *eth_pkt, unsigned int payload_size, unsigned int new_fcs)
{
    if(eth_pkt->type == 0x8100){
        vlan_ethernet_frame_t *vlan_eth_pkt = (vlan_ethernet_frame_t *)eth_pkt;
        vlan_eth_pkt->FCS = new_fcs;
    }
    else{
        eth_pkt->FCS = new_fcs;
    }
}
#if 0
static inline void SET_COMMON_ETH_FCS(ethernet_frame_t *frame, unsigned int payload_size, unsigned int new_fcs)
{
    ETH_FCS(frame, payload_size) = new_fcs;
}
#endif

// TBR: add ETH header to the left side of data
static inline ethernet_frame_t *ALLOC_ETH_HRD_WITH_PAYLOAD(char *pkt, unsigned int pkt_size)
{
    /* Copy the data in a local buffer */
    char *ptr = calloc(0, pkt_size);
    memcpy(ptr, pkt, pkt_size);

    /* ptr - 6 Bytes of dst MAC - 6 Bytes of src MAC - payload */
    ethernet_frame_t *frame = (ethernet_frame_t *)(pkt - ETH_HDR_SIZE_EXCL_PAYLOAD_FCS);
    memset((char *)frame, 0, ETH_HDR_SIZE_EXCL_PAYLOAD_FCS);

    memcpy((char *)frame->payload, ptr, pkt_size);
    SET_COMMON_ETH_FCS(frame, pkt_size, 0);

    free(ptr);
    return frame;
}

static inline unsigned int GET_802_1Q_VLAN_ID(vlan_8021q_hdr_t *vlan_8021q_hdr)
{
    return (unsigned int)vlan_8021q_hdr->tci_vid;
}

static inline vlan_8021q_hdr_t *is_pkt_vlan_tagged(ethernet_frame_t *eth_pkt)
{
    vlan_8021q_hdr_t *vlan_hdr = NULL;
    if(eth_pkt->type == 0x8100){
        memcpy(vlan_hdr, &eth_pkt->type, sizeof(vlan_8021q_hdr_t));
        return vlan_hdr;
    }
    return vlan_hdr;
}

static inline bool_t l2_frame_recv_qualify_on_interface(interface_t *intf, ethernet_frame_t *frame, unsigned int *output_vlan_id)
{
    unsigned int intf_vlan_id = 0U;
    unsigned int pkt_vlan_id = 0U;
    vlan_8021q_hdr_t *vlan_hdr = is_pkt_vlan_tagged(frame);

    if(vlan_hdr){
        printf("Info: Pkt vlan tagged\n");
        intf_vlan_id = get_access_intf_operating_vlan_id(intf);
        pkt_vlan_id = GET_802_1Q_VLAN_ID(vlan_hdr);
    }

    /* case 10: if receiving interface is neither in L3 mode or L2 mode, reject the packet */
    if (!IS_INTF_L3_MODE(intf) && IF_L2_MODE(intf) == L2_MODE_UNKNOWN){
        printf("Error: Receiving Interface neither in L2 or L3 mode\n");
        return FALSE;
    }

    /* Case 3 & 4: */
    /* If interfaces is in ACCESS mode, but no vlan id is configured, this is an invalid/incomplete configuration
       Such a node should not accept both tagged and untagged packets */
    if(!IS_INTF_L3_MODE(intf) && IF_L2_MODE(intf) == ACCESS && get_access_intf_operating_vlan_id(intf) == 0U){
        printf("Error: Receiving Interface is in access mode, but vlan id not configured\n");
        return FALSE;
    }

    /* Case 6 & 5: If interface is is access mode and vlan id is configured, then
        1. It should accept untagged frame and tag it with its vlan id 
        2. it should accept only frames tagged with its vlan id 
    */
    if(!IS_INTF_L3_MODE(intf) && IF_L2_MODE(intf) == ACCESS)
    {
        /* Case 6: Packet not tagged , but vlan id assigned for interface */
        if(!vlan_hdr && intf_vlan_id)
        {
            /* update the vlan so that the pkt is tagged by the calling function */
            *output_vlan_id = intf_vlan_id;
            return TRUE;
        }
        /* Case 5: If pkt is tagged, accept only if the vlan ids match */
        if(pkt_vlan_id == intf_vlan_id){
            return TRUE;
        }
        else{
            printf("Error: Receiving Interface in ACCESS mode does not have a vlan id match\n");
            return FALSE;
        }
    }

    /* Case 7 & 8: If interface is operating in trunk mode, it should discard all untagged packets */
    if(!IS_INTF_L3_MODE(intf) && IF_L2_MODE(intf) == TRUNK){
        if(!vlan_hdr){
            printf("Error: Receiving Interface in TRUNK mode, received untagged pkt\n");
            return FALSE;
        }
    }

    /* Case 9 : if interface is operating in TRUNK mode, it must accept frame with any of the configured vlan id */
    if(!IS_INTF_L3_MODE(intf) && IF_L2_MODE(intf) == TRUNK && vlan_hdr){
        if(is_trunk_intf_vlan_enabled(intf, pkt_vlan_id))
            return TRUE;
        else{
            printf("Error: Receiving Interface in TRUNK mode, received vlan id not configured\n");
            return FALSE;
        }
    }

    /* If the interface is operating in L3 mode */
    if(IS_INTF_L3_MODE(intf)){
        /* Case 2: If vlan tagged packed received by L3 mode interface, discard the pkt */
        if(vlan_hdr){
            printf("Error: Receiving Interface in L3 mode, received a tagged pkt\n");
            return FALSE;
        }

        /* case 1: No vlan tagged frame, accept only if mac address matches */
        if (memcmp(IF_MAC(intf), frame->dst_mac.mac_addr, sizeof(mac_add_t)) == 0U)
            return TRUE;

        /* Case 1: if untagged frame, and mac is broadcast address */
        if (IS_MAC_BROADCAST_ADDR(frame->dst_mac.mac_addr)) // some memory issue found. need to debug.
        {
            printf("Info: Allow untagged pkt Mac broadcast in L3 mode\n");
            return TRUE;
        }
    }
    return FALSE;
}

static inline char *GET_ETHERNET_HDR_PAYLOAD(ethernet_frame_t *eth_pkt)
{
    if(eth_pkt->type == 0x8100U){
        vlan_ethernet_frame_t *vlan_eth_pkt = (vlan_ethernet_frame_t *)&eth_pkt;
        return (char *)&vlan_eth_pkt->payload;
    }
    else{
        return (char *)&eth_pkt->payload;
    }
}

static inline unsigned int GET_ETH_HDR_SIZE_EXCL_PAYLOAD(ethernet_frame_t *eth_pkt)
{
    if(is_pkt_vlan_tagged(eth_pkt)){
        return VLAN_ETH_HDR_SIZE_EXCL_PAYLOAD;
    }
    else
        return ETH_HDR_SIZE_EXCL_PAYLOAD;
}


#endif // LAYER_2_H
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"
#include "cmdcodes.h"
#include "graph.h"
#include <stdio.h>


extern graph_t *topo;


/* Display function when user presses ? */
void display_graph_nodes(param_t *param, ser_buff_t *tlv_buf){
    node_t *node;
    glthread_t *curr;
    ITERATE_GLTHREAD_BEGIN(&node->graph_glue, curr){
        node = graph_glue_to_node(curr);
        printf("%s\n", node->node_name);
    }ITERATE_GLTHREAD_END(&node->graph_glue, curr);
}

/*Generic Topology Commands*/
static int
show_nw_topology_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE = -1;
    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    switch(CMDCODE){

        case CMDCODE_SHOW_NW_TOPOLOGY:
            dump_nw_graph(topo);
            break;
        default:
            ;
    }
    return 0;
}

extern void send_arp_broadcast_request(node_t *node, interface_t *oif, char * ip_addr);
static int arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    char *ip_addr;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_addr = tlv->value;
    } TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);
    send_arp_broadcast_request(node, NULL, ip_addr);
    return 0;
}

/* General Validations */
int validate_node_existence(char *node_name)
{
    node_t *node = get_node_by_node_name(topo, node_name);
    if(node)
        return VALIDATION_SUCCESS;
    else{
        printf("Error: Node %s does not exist\n", node_name);
        return VALIDATION_FAILED;
    }
}

int
validate_mask_value(char *mask_str){

    unsigned int mask = atoi(mask_str);
    if(!mask){
        printf("Error : Invalid Mask Value\n");
        return VALIDATION_FAILED;
    }
    if(mask >= 0 && mask <= 32)
        return VALIDATION_SUCCESS;
    return VALIDATION_FAILED;
}

/*Layer 2 Commands*/

typedef struct arp_table_ arp_table_t;
extern void dump_arp_table(arp_table_t *arp_table);


static int show_arp_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U){
            node_name = tlv->value;
        }
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_arp_table(NODE_ARP_TABLE(node));
    return 0;
}

extern void dump_mac_table(mac_table_t *mac_table);
static int show_mac_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U){
            node_name = tlv->value;
        }
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_mac_table(NODE_MAC_TABLE(node));
    return 0;
}

/*Layer 3 Commands*/
extern void layer5_ping_fn(node_t *node, char *dst_ip_addr);
extern void layer5_ero_ping_fn(node_t *node, char *dst_ip_addr, char *ero_ip_addr);

static int ping_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    int CMDCODE;
    node_t *node;
    char *ip_addr = NULL, 
         *ero_ip_addr = NULL;
    char *node_name;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    tlv_struct_t *tlv = NULL;
 
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if     (strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            ip_addr = tlv->value;
        else if(strncmp(tlv->leaf_id, "ero-ip-address", strlen("ero-ip-address")) ==0)
            ero_ip_addr = tlv->value;
        else
            assert(0);
    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);

    switch(CMDCODE){

        case CMDCODE_PING:
            layer5_ping_fn(node, ip_addr);
            break;
        case CMDCODE_ERO_PING:
            layer5_ero_ping_fn(node, ip_addr, ero_ip_addr);
        default:
            ;
    }
    return 0;
}

extern void dump_rt_table(rt_table_t **rt_table);
static int show_rt_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){
    node_t * node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
    }TLV_LOOP_END(tlv_buf, tlv);

    node = get_node_by_node_name(topo, node_name);
    dump_rt_table(NODE_RT_TABLE(node));
    return 0;
}

static int
l3_config_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable){

    node_t *node = NULL;
    char *node_name = NULL;
    char *intf_name = NULL;
    char *gwip = NULL;
    char *mask_str = NULL;
    char *dest = NULL;
    int CMDCODE = -1;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf); 
    
    tlv_struct_t *tlv = NULL;
    
    TLV_LOOP_BEGIN(tlv_buf, tlv){

        if     (strncmp(tlv->leaf_id, "node-name", strlen("node-name")) ==0)
            node_name = tlv->value;
        else if(strncmp(tlv->leaf_id, "ip-address", strlen("ip-address")) ==0)
            dest = tlv->value;
        else if(strncmp(tlv->leaf_id, "gw-ip", strlen("gw-ip")) ==0)
            gwip = tlv->value;
        else if(strncmp(tlv->leaf_id, "mask", strlen("mask")) ==0)
            mask_str = tlv->value;
        else if(strncmp(tlv->leaf_id, "oif", strlen("oif")) ==0)
            intf_name = tlv->value;
        else
            assert(0);

    }TLV_LOOP_END;

    node = get_node_by_node_name(topo, node_name);

    char mask;
    if(mask_str){
        mask = atoi(mask_str);
    }

    switch(CMDCODE){
        case CMDCODE_CONF_NODE_L3ROUTE:
            switch(enable_or_disable){
                case CONFIG_ENABLE:
                {
                    interface_t *intf;
                    if(intf_name){
                        intf = get_node_intf_by_name(node, intf_name);
                        if(!intf){
                            printf("Config Error : Non-Existing Interface : %s\n", intf_name);
                            return -1;
                        }
                        if(!IS_INTF_L3_MODE(intf)){
                            printf("Config Error : Not L3 Mode Interface : %s\n", intf_name);
                            return -1;
                        }
                    }
                    rt_table_add_route(NODE_RT_TABLE(node), dest, mask, gwip, intf_name);
                }
                break;
                case CONFIG_DISABLE:
                    delete_rt_table_entry(NODE_RT_TABLE(node), dest, mask);
                    break;
                default:
                    ;
            }
            break;
        default:
            break;
    }
    return 0;
}

void nw_init_cli(void)
{
    init_libcli();

    param_t *show       = libcli_get_show_hook();
    param_t *debug      = libcli_get_debug_hook();
    param_t *config     = libcli_get_config_hook();
    param_t *run        = libcli_get_run_hook();
    param_t *debug_show = libcli_get_debug_show_hook();
    param_t *root       = libcli_get_root();

    /* show topology */
    {
        static param_t topology;
        init_param(&topology, CMD, "topology", show_nw_topology_handler, 0, INVALID, 0, "Dump complete n/w topology");
        libcli_register_param(show, &topology);
        set_param_cmd_code(&topology, CMDCODE_SHOW_NW_TOPOLOGY);

        /* Show node */
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(show, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /* show node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                {
                    /* Show node <node_name> arp */
                    static param_t arp;
                    init_param(&arp, CMD, "arp", show_arp_handler, 0, INVALID, 0, "Dump Arp Table");
                    libcli_register_param(&node_name, &arp);
                    set_param_cmd_code(&arp, CMDCODE_SHOW_NODE_ARP_TABLE);
                }
                {
                    /* show node <node_name> mac */
                    static param_t mac;
                    init_param(&mac, CMD, "mac", show_mac_handler, 0, INVALID, 0, "Dump Mac Table");
                    libcli_register_param(&node_name, &mac);
                    set_param_cmd_code(&mac, CMDCODE_SHOW_NODE_MAC_TABLE);
                }
                {
                    /* show node <node_name> rt */
                    static param_t rt;
                    init_param(&rt, CMD, "rt", show_rt_handler, 0, INVALID, 0, "Dump L3 Routing Table");
                    libcli_register_param(&node_name, &rt);
                    set_param_cmd_code(&rt, CMDCODE_SHOW_NODE_RT_TABLE);
                }                
            }
        }
    }

    /* run node */
    {
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(run, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /* run node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                /* run node <node_name> ping */
                static param_t ping;
                init_param(&ping, CMD, "ping", 0, 0, INVALID, 0, "Ping Utility");
                libcli_register_param(&node_name, &ping);
                {
                    /* run node <node_name> ping <ip-address> */
                    static param_t ip_addr;
                    init_param(&ip_addr, LEAF, 0, ping_handler, 0, IPV4, "ip-address", "Ipv4 Address");
                    libcli_register_param(&ping, &ip_addr);
                    set_param_cmd_code(&ip_addr, CMDCODE_PING);
                    {
                        static param_t ero;
                        init_param(&ero, CMD, "ero", 0, 0, INVALID, 0, "ERO(Explicit Route Object)");
                        libcli_register_param(&ip_addr, &ero);
                        {
                            static param_t ero_ip_addr;
                            init_param(&ero_ip_addr, LEAF, 0, ping_handler, 0, IPV4, "ero-ip-address", "ERO Ipv4 Address");
                            libcli_register_param(&ero, &ero_ip_addr);
                            set_param_cmd_code(&ero_ip_addr, CMDCODE_ERO_PING);
                        }
                    }
                }

                /* run node <node_name> resolve-arp */
                static param_t resolve_arp;
                init_param(&resolve_arp, CMD, "resolve-arp", 0, 0, INVALID, 0, "Resolve ARP");
                libcli_register_param(&node_name, &resolve_arp);
                {
                    /* run node <node_name> resolve-arp <ip-address> */
                    static param_t ip_addr;
                    init_param(&ip_addr, LEAF, 0, arp_handler, 0, IPV4, "ip-address", "Nbr IPv4 Address");
                    libcli_register_param(&resolve_arp, &ip_addr);
                    set_param_cmd_code(&ip_addr, CMDCODE_RUN_ARP);
                }
            }
        }
    }

    /* config node */
    {
        static param_t node;
        init_param(&node, CMD, "node", 0, 0, INVALID, 0, "\"node\" keyword");
        libcli_register_param(config, &node);
        libcli_register_display_callback(&node, display_graph_nodes);
        {
            /*config node <node_name> */
            static param_t node_name;
            init_param(&node_name, LEAF, 0, 0, validate_node_existence, STRING, "node-name", "Node Name");
            libcli_register_param(&node, &node_name);
            {
                /* config node <node_name> interface */
                {
                    /* config node <node_name> interface <if-name> */
                    {
                        /* config node <node_name> interface <if-name> l2mode */
                        {
                            /* config node <node_name> interface <if-name> l2mode <access|trunk> */
                        }
                        /* config node <node_name> interface <if-name> vlan */
                        {
                            /* config node <node_name> interface <if-name> vlan <vlan-id> */
                        }
                    }
                }
                /* config node <node_name> route */
                {
                    static param_t route;
                    init_param(&route, CMD, "route", 0, 0, INVALID, 0, "L3 route");
                    libcli_register_param(&node_name, &route);

                    /* config node <node_name> route <ip-address> */
                    {
                        static param_t ip_addr;
                        init_param(&ip_addr, LEAF, 0, 0, 0, IPV4, "ip-address", "IPv4 Address");
                        libcli_register_param(&route, &ip_addr);

                        /* config node <node_name> route <ip-address> <mask> */
                        {
                            static param_t mask;
                            init_param(&mask, LEAF, 0, l3_config_handler, validate_mask_value, INT, "mask", "mask(0-32");
                            libcli_register_param(&ip_addr, &mask);
                            set_param_cmd_code(&mask, CMDCODE_CONF_NODE_L3ROUTE);

                            /* config node <node_name> route <ip-address> <mask> <gw-ip> */
                            {
                                static param_t gwip;
                                init_param(&gwip, LEAF, 0, 0, 0, IPV4, "gw-ip", "IPv4 Address");
                                libcli_register_param(&mask, &gwip);

                                /* config node <node_name> toute <ip-address> <mask> <gw-ip> <oif> */
                                {
                                    static param_t oif;
                                    init_param(&oif, LEAF, 0, l3_config_handler, 0, STRING, "oif", "Out-going intf Name");
                                    libcli_register_param(&gwip, &oif);
                                    set_param_cmd_code(&oif, CMDCODE_CONF_NODE_L3ROUTE);
                                }
                            }
                        }
                    }
                }
            }
            support_cmd_negation(&node_name);
        }
    }
    support_cmd_negation(config);
}
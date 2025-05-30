#ifndef GRAPH_H
#define GRAPH_H

#include "glueThread/glthread.h"
#include "net.h"
#include <assert.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define IF_NAME_SIZE       16U
#define NODE_NAME_SIZE     16U
#define TOPOLOGY_NAME_SIZE 32U
#define MAX_IF_PER_NODE    4U

/* Forward Declaration */
typedef struct node_ node_t;
typedef struct link_ link_t;

typedef struct interface_{
    char if_name[IF_NAME_SIZE];
    struct node_ *att_node;
    struct link_ *link;
    intf_nw_prop_t intf_nw_prop;
}interface_t;

struct link_{
    interface_t intf1;
    interface_t intf2;
    unsigned int cost;
};

typedef struct spf_data_ spf_data_t;
struct node_{
    char node_name[NODE_NAME_SIZE];
    interface_t *intf[MAX_IF_PER_NODE];
    unsigned int udp_port_number;
    int udp_sock_fd;
    node_nw_prop_t node_nw_prop;
    /* SPF calculation */
    spf_data_t *spf_data;
    glthread_t graph_glue;
};

typedef struct graph_{
    char topology_name[TOPOLOGY_NAME_SIZE];
    glthread_t node_list;
}graph_t;

GLTHREAD_TO_STRUCT(graph_glue_to_node, node_t, graph_glue);

graph_t *create_new_graph(char *topology_name);
node_t *create_graph_node(graph_t *graph, char * node_name);
void insert_link_between_two_nodes(node_t *node1, node_t *node2, char *from_if_name, 
                                    char *to_if_name, unsigned int cost);
void dump_graph(graph_t *graph);
void dump_node(node_t *node);
void dump_interface(interface_t *interface);

void init_udp_socket(node_t *node);


/* Returns the Neighbour node of the interface */
static inline node_t *get_nbr_node(interface_t *interface);

/* Return an interface of a node, which is empty (no link established)
   return -1 if no such interface is available */
static inline int get_node_intf_available_slot(node_t *node);

static inline interface_t *get_node_intf_by_name(node_t *node, char *if_name);

static inline node_t *get_node_by_node_name(graph_t *topo, char *node_name);

static inline node_t *get_nbr_node(interface_t *interface)
{
    assert(interface->att_node);
    assert(interface->link);
    link_t *link = interface->link;
    if(&link->intf1 == interface)
        return link->intf2.att_node;
    else
        return link->intf1.att_node;
}

static inline int get_node_intf_available_slot(node_t *node)
{
    int i;
    interface_t *intf;
    for(i = 0U; i <  MAX_IF_PER_NODE; i++)
    {
        if(node->intf[i] == NULL)
            return i;
    }
    return -1;
}

static inline interface_t *get_node_intf_by_name(node_t *node, char *if_name)
{
    unsigned int i;
    interface_t *interface;
    for(i = 0U; i < MAX_IF_PER_NODE; i++)
    {
        interface = node->intf[i];
        //printf("interface #%d with address %x in node %s\n", i, interface, node->node_name);
        if(!interface)
            return NULL;
        if(strncmp(interface->if_name, if_name, IF_NAME_SIZE) == 0U)
            return interface;
    }
    return NULL;
}

static inline node_t *get_node_by_node_name(graph_t *topo, char *node_name)
{
    node_t *node;
    glthread_t *curr;
    ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr){
        node = graph_glue_to_node(curr);
        if(strncmp(node->node_name, node_name, NODE_NAME_SIZE) == 0U)
            return node;
    }ITERATE_GLTHREAD_END(&topo->node_list, curr);
    return NULL;
}

#endif //GRAPH_H
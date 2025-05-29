#ifndef LAYER3_H
#define LAYER3_H

#include <stdint.h>
#include <string.h>
#include "../glueThread/glthread.h"
#include "../graph.h"
#include "../net.h"
#include "../Layer2/layer2.h"
#include "../utils.h"

typedef struct rt_table_{
    glthread_t route_list;
}rt_table_t;

typedef struct l3_route_{
    char dest_ip[16];    //key
    char mask_dest_ip;   //key
    bool_t is_direct;    // if set to TRUE, gw_ip and oif is not relevant
    char gw_ip[16];      // next hop ip
    char oif[IF_NAME_SIZE];   // out interface name
    glthread_t rt_glue;
}l3_route_t;

GLTHREAD_TO_STRUCT(rt_glue_to_l3_route, l3_route_t, rt_glue);

void init_rt_table(rt_table_t **rt_table);
extern void rt_table_add_direct_route(rt_table_t *rt_table, char *dst_ip, char mask);
void rt_table_add_route(rt_table_t *rt_table, char *dst_ip, char mask, char *gw_ip, char *oif);
l3_route_t *rt_table_lookup(rt_table_t *rt_table, char *ip_addr, char mask);
l3_route_t *l3rib_lookup_lpm(rt_table_t *rt_table, uint32_t dst_ip);
void dump_rt_table(rt_table_t *rt_table);
void delete_rt_table_entry(rt_table_t *rt_table, char *ip_addr, char mask);
void clear_rt_tabel(rt_table_t *rt_table);

#define IS_L3_ROUTES_EQUAL(rt1, rt2)              \
    ((strncmp(rt1->dest_ip, rt2->dest_ip, 16) == 0) &&  \
    (rt1->mask_dest_ip == rt2->mask_dest_ip) &&                   \
    (rt1->is_direct == rt2->is_direct) &&         \
    (strncmp(rt1->gw_ip, rt2->gw_ip, 16) == 0) && \
    strncmp(rt1->oif, rt2->oif, IF_NAME_SIZE) == 0)
    
#endif
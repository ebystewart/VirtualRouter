#include <stdio.h>
#include <stdint.h>
#include "../../tcp_public.h"

#define INFINITE_METRIC 0xFFFFFFFFUL
#define spf_data_offset_from_priority_thread_glue \
        ((size_t)&(((spf_data_t *)0)->priority_thread_glue))
#define SPF_METRIC(node_ptr) (node_ptr->spf_data->spf_metric)

extern graph_t *topo;

typedef struct spf_data_{
    node_t *node;
    glthread_t spf_result_head;
    /* fileds use for spf calculation */
    uint32_t spf_metric;
    glthread_t priority_thread_glue;
    nexthop_t *nexthops[MAX_NXT_HOPS];
}spf_data_t;

GLTHREAD_TO_STRUCT(priority_thread_glue_to_spf_data, spf_data_t, priority_thread_glue);

typedef struct spf_result_{
    node_t *node;
    uint32_t spf_metric;
    nexthop_t *nexthops[MAX_NXT_HOPS];
    glthread_t spf_result_glue;
}spf_result_t;

GLTHREAD_TO_STRUCT(spf_result_glue_to_spf_result, spf_result_t, spf_result_glue);

static void compute_spf(node_t *spf_root);
static int spf_comparison_fn(void *data1, void *data2);
static int spf_install_routes(node_t *spf_root);
static void spf_record_result(node_t *spf_root, node_t *processed_node);
static void spf_explore_nbrs(node_t *spf_root, node_t *curr_node, glthread_t *priority_lst);

static void show_spf_results(node_t *node)
{
    printf("%s: called\n", __FUNCTION__);
}

int spf_algo_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable)
{
    int CMDCODE;
    node_t *node;
    char *node_name;
    tlv_struct_t *tlv = NULL;

    CMDCODE = EXTRACT_CMD_CODE(tlv_buf);

    TLV_LOOP_BEGIN(tlv_buf, tlv){
        if(strncmp(tlv->leaf_id, "node-name", strlen("node-name")) == 0U){
            node_name = tlv->value;
        }
    }TLV_LOOP_END;

    if(node_name){
        node = get_node_by_node_name(topo, node_name);
    }
    switch(CMDCODE)
    {
        case CMDCODE_SHOW_SPF_RESULTS:
        {
            show_spf_results(node);
            break;
        }
        case CMDCODE_RUN_SPF:
        {
            compute_spf(node);
            break;
        }
        default:
            break;
    }
    return 0;
}

void spf_flush_nexthops(nexthop_t **nexthop)
{
    uint32_t idx = 0U;

    if(!nexthop)
        return;

    for( ; idx < MAX_NXT_HOPS; idx++){
        if(nexthop[idx]){
            assert(nexthop[idx]->ref_count);
            nexthop[idx]->ref_count--;
            if(nexthop[idx]->ref_count == 0U){
                free(nexthop[idx]);
            }
            nexthop[idx] = NULL;
        }
    }
}

static inline void free_spf_result(spf_result_t *spf_result)
{
    spf_flush_nexthops(spf_result->nexthops);
    remove_glthread(&spf_result->spf_result_glue);
    free(spf_result);
}

static nexthop_t *create_new_nexthop(interface_t *oif)
{
    nexthop_t *nexthop = calloc(1, sizeof(nexthop_t));
    nexthop->oif = oif;
    interface_t *neighbour_intf = &oif->link->intf1 == oif ? &oif->link->intf2 : &oif->link->intf1;
    if(!neighbour_intf){
        free(nexthop);
        return NULL;
    }
    strncpy(nexthop->gw_ip, IF_IP(neighbour_intf), 16U);
    nexthop->ref_count = 0U;
    return nexthop;
}

static bool_t spf_insert_new_nexthop(nexthop_t **nexthop_array, nexthop_t *nexthop)
{
    uint32_t idx = 0U;
    for( ; idx < MAX_NXT_HOPS; idx++){
        if(nexthop_array[idx])
            continue;
        nexthop_array[idx] = nexthop;
        nexthop_array[idx]->ref_count++;
        return TRUE;
    }
    return FALSE;
}

static bool_t spf_is_nexthop_exist(nexthop_t **nexthop_array, nexthop_t *nexthop)
{
    uint32_t idx = 0U;
    for( ; idx < MAX_NXT_HOPS; idx++){
        /* Handle NULL pointers */
        if(!nexthop_array[idx])
            continue;
        if(nexthop_array[idx]->oif == nexthop->oif){
            return TRUE;
        }
    }
    return FALSE;
}

static int spf_union_nexthops_arrays(nexthop_t **src, nexthop_t **dst)
{

    uint32_t src_idx = 0U;
    uint32_t dst_idx = 0U;
    uint32_t copied_count = 0U;

    /* move formward to the start of copy location */
    while(dst[dst_idx] && dst_idx < MAX_NXT_HOPS){
        dst_idx++;
    }

    /* Return if there is no space for new nexthops */
    if(dst_idx == (MAX_NXT_HOPS - 1U))
        return 0;

    for( ; src_idx < MAX_NXT_HOPS && dst_idx < MAX_NXT_HOPS; src_idx++, dst_idx++){
        if(src[src_idx] && (spf_is_nexthop_exist(dst, src[src_idx]) ==  FALSE)){
            dst[dst_idx] = src[src_idx];
            dst[dst_idx]->ref_count++;
            copied_count++;
        }
    }
    return copied_count;
}

static int spf_comparison_fn(void *data1, void *data2)
{
    spf_data_t *spf_data1 = (spf_data_t *)data1;
    spf_data_t *spf_data2 = (spf_data_t *)data2;

    if(spf_data1->spf_metric < spf_data2->spf_metric)
        return -1;
    else if(spf_data1->spf_metric > spf_data2->spf_metric)
        return 1;
    else
        return 0;
}

static spf_result_t *spf_lookup_spf_result_by_node(node_t *spf_root, node_t *node)
{
    glthread_t *curr;
    spf_result_t *spf_result;

    ITERATE_GLTHREAD_BEGIN(&spf_root->spf_data->spf_result_head, curr){
        spf_result = spf_result_glue_to_spf_result(curr);
        if(spf_result->node == node){
            return spf_result;
        }
    }ITERATE_GLTHREAD_END(&spf_root->spf_data->spf_result_head, curr);
    return NULL;
}

static void init_node_spf_data(node_t *node, bool_t delete_spf_result)
{
    if(!node->spf_data){
        node->spf_data = calloc(1, sizeof(spf_data_t));
        init_glthread(&node->spf_data->spf_result_head);
        node->spf_data->node = node;
    }
    else if(delete_spf_result){
        glthread_t *curr;
        ITERATE_GLTHREAD_BEGIN(&node->spf_data->spf_result_head, curr){
            spf_result_t *spf_result = spf_result_glue_to_spf_result(curr);
            free_spf_result(spf_result);
        }ITERATE_GLTHREAD_END(&node->spf_data->spf_result_head, curr);
        init_glthread(&node->spf_data->spf_result_head);
    }

    SPF_METRIC(node) = INFINITE_METRIC;
    remove_glthread(&node->spf_data->priority_thread_glue);
    spf_flush_nexthops(node->spf_data->nexthops);
}

void initialize_direct_nbrs(node_t *root_node)
{
    node_t *nbr = NULL;
    char * next_hop_ip = NULL;
    interface_t *oif = NULL;
    nexthop_t *nexthop = NULL;

    ITERATE_NODE_NBRS_BEGIN(root_node, nbr, oif, next_hop_ip){

        /* Check if the link is bidirectional */
        if(is_interface_l3_bidirectional(oif))
            continue;
        /* populate nexthop array of direct neighbours */
        if(get_link_cost(oif) < SPF_METRIC(nbr)){
            spf_flush_nexthops(nbr->spf_data->nexthops);
            nexthop = create_new_nexthop(oif);
            spf_insert_new_nexthop(nbr->spf_data->nexthops, nexthop);
            SPF_METRIC(nbr) = get_link_cost(oif);
        }
        else if (get_link_cost(oif) == SPF_METRIC(nbr)){
            nexthop = create_new_nexthop(oif);
            spf_insert_new_nexthop(nbr->spf_data->nexthops, nexthop);
        }
    } ITERATE_NODE_NBRS_END(root_node, nbr, oif, next_hop_ip);
}

static void compute_spf(node_t *spf_root)
{
    glthread_t *curr;
    node_t *node;
    node_t *nbr;
    interface_t *oif;
    spf_data_t *curr_spf_data;
    char *nxt_hop_ip = NULL;

    init_node_spf_data(spf_root, TRUE);
    SPF_METRIC(spf_root) = 0U;

    ITERATE_GLTHREAD_BEGIN(&topo->node_list, curr){
        node = graph_glue_to_node(curr);
        /* skip root node */
        if(node == spf_root)
            continue;
        init_node_spf_data(node, FALSE);
    }ITERATE_GLTHREAD_END(&topo->node_list, curr);

    /* initialize direct neighbours */
    initialize_direct_nbrs(spf_root);

    /* initialize the priority queue */
    glthread_t *priority_lst;
    init_glthread(&priority_lst);
    /* insert The spf_root as the only node in to PQ to begin with */
    glthread_priority_insert(&priority_lst, &spf_root->spf_data->priority_thread_glue, \
                             spf_comparison_fn, spf_data_offset_from_priority_thread_glue);

    while(!IS_GLTHREAD_LIST_EMPTY(priority_lst)){
        curr = dequeue_glthread_first(&priority_lst);
        curr_spf_data = priority_thread_glue_to_spf_data(curr);

        if(curr_spf_data->node == spf_root){
            ITERATE_NODE_NBRS_BEGIN(curr_spf_data->node, nbr, oif, nxt_hop_ip){
                if(is_interface_l3_bidirectional(oif))
                    continue;
                if(IS_GLTHREAD_LIST_EMPTY(&nbr->spf_data->priority_thread_glue)){
                    glthread_priority_insert(&priority_lst, &nbr->spf_data->priority_thread_glue, \
                                             spf_comparison_fn, spf_data_offset_from_priority_thread_glue);
                }
            }ITERATE_NODE_NBRS_END(curr_spf_data->node, nbr, oif, nxt_hop_ip);
            continue;
        }
        /* record result */
        spf_record_result(spf_root, curr_spf_data->node);

        spf_explore_nbrs(spf_root, curr_spf_data->node, &priority_lst);
    }

    /* Calculate final routing table from spf_result */
    int count = spf_install_routes(spf_root);
}

static int spf_install_routes(node_t *spf_root)
{
    rt_table_t *rt_table = NODE_RT_TABLE(spf_root);

    /* clear all routes except direct routes */
    clear_rt_table(rt_table);

    /* now iterate over result list and install routes for loopback address for all routers */
    int i = 0;
    int count = 0;
    glthread_t *curr;
    spf_result_t *spf_result;
    nexthop_t *nexthop = NULL;

    ITERATE_GLTHREAD_BEGIN(&spf_root->spf_data->spf_result_head, curr){
        spf_result = spf_result_glue_to_spf_result(curr);
        for(i = 0; i < MAX_NXT_HOPS; i++){
            nexthop = spf_result->nexthops[i];
            if(!nexthop)
                continue;
            //rt_table_add_route(rt_table, NODE_LO_ADDRESS(spf_result->node), 32U, nexthop->gw_ip, nexthop->oif, spf_result->spf_metric);
            count++;
        }
    }ITERATE_GLTHREAD_END(&spf_root->spf_data->spf_result_head, curr);

    return count;
}

static void spf_record_result(node_t *spf_root, node_t *processed_node)
{
    /* we have dequeued the processed node from the priority queue after 
       determining it as the least cost node to the root node */
    /* check if the result is lready stored */
    assert(!spf_lookup_spf_result_by_node(spf_root, processed_node));

    /* allocate memory for spf result */
    spf_result_t *spf_result = calloc(1, sizeof(spf_result_t));

    /* The following parameters become part of the spf result 
     * - the node itself   
     * - the shortest path cost to reach the node
     * - The set of nexthops for this node
    */
    spf_result->node = processed_node;
    spf_result->spf_metric = processed_node->spf_data->spf_metric;
    spf_union_nexthops_arrays(processed_node->spf_data->nexthops, spf_result->nexthops);

    /* add the created and updated result data structure to the spf result table
       in the spf root */
    init_glthread(&spf_result->spf_result_glue);
    glthread_add_next(&spf_root->spf_data->spf_result_head, &spf_result->spf_result_glue);
}

static void spf_explore_nbrs(node_t *spf_root, node_t *curr_node, glthread_t *priority_lst)
{
    node_t *nbr;
    interface_t *oif;
    char *nxt_hop_ip = NULL;

    ITERATE_NODE_NBRS_BEGIN(curr_node, nbr, oif, nxt_hop_ip){
        if(!is_interface_l3_bidirectional(oif))
            continue;
        if((SPF_METRIC(curr_node) + get_link_cost(oif)) < SPF_METRIC(nbr)){
            /* remove the obsolete nexthops */
            spf_flush_nexthops(nbr->spf_data->nexthops);

            /* copy the new set of nexthops from the predecessor node
               from which shortest path to the nbe node is just explored */
            spf_union_nexthops_arrays(curr_node->spf_data->nexthops, nbr->spf_data->nexthops);

            /* update shortest path cost of neighbour node */
            SPF_METRIC(nbr) = SPF_METRIC(curr_node) + get_link_cost(oif);

            /* if the nbr node is already present in PQ, remove it from PQ and add it back,
               so that it takes correct position in the PQ as per the new spf metric */
            if(!IS_GLTHREAD_LIST_EMPTY(&nbr->spf_data->priority_thread_glue))
                remove_glthread(&nbr->spf_data->priority_thread_glue);
            
            glthread_priority_insert(&priority_lst, &nbr->spf_data->priority_thread_glue, \
                                    spf_comparison_fn, spf_data_offset_from_priority_thread_glue);
        }
        /* In case of ECMP */
        else if((SPF_METRIC(curr_node) + get_link_cost(oif)) == SPF_METRIC(nbr)){
            spf_union_nexthops_arrays(curr_node->spf_data->nexthops, nbr->spf_data->nexthops);
        }
    }ITERATE_NODE_NBRS_END(curr_node, nbr, oif, nxt_hop_ip);

    /* If we are done processing the curr node, remove its nexthops and lower its ref_count */
    spf_flush_nexthops(curr_node->spf_data->nexthops);
}

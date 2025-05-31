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

void compute_spf(node_t *spf_root)
{
    printf("%s: called\n", __FUNCTION__);
}

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

static int spf_comparision_fn(void *data1, void *data2)
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

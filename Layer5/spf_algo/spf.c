#include <stdio.h>
#include <stdint.h>
#include "../../tcp_public.h"

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
#ifndef __TCP_IP_TRACE_H__
#define __TCP_IP_TRACE_H__

#include <stdio.h>
#include <stdbool.h>

#define TCP_PRINT_BUFFER_SIZE 1024
#define TCP_GET_NODE_SEND_LOG_BUFFER(node) (node->node_nw_prop.send_log_buffer)

typedef struct node_ node_t;
typedef struct interface_ interface_t;
typedef bool bool_t;

typedef enum{

    ETH_HDR,
    IP_HDR
}hdr_type_t;

typedef struct log_{

    bool_t all;
    bool_t recv;
    bool_t send;
    bool_t is_stdout;
    bool_t l3_fwd;
    FILE *log_file;
}log_t;

#endif
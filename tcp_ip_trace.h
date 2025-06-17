#ifndef __TCP_IP_TRACE_H__
#define __TCP_IP_TRACE_H__

#include <stdio.h>
#include <stdbool.h>
#include "CommandParser/libcli.h"
#include "CommandParser/cmdtlv.h"

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


void tcp_ip_init_node_log_info(node_t *node);
void tcp_ip_set_all_log_info_params(log_t *log_info, bool_t status);
void tcp_ip_init_intf_log_info(interface_t *intf);
void tcp_ip_show_log_status(node_t *node);
void tcp_dump_recv_logger(node_t *node, interface_t *intf, char *pkt, uint32_t pkt_size, hdr_type_t hdr_type);
void tcp_dump_l3_fwding_logger(node_t *node, char *oif_name, char *gw_ip);
void tcp_dump_send_logger(node_t *node, interface_t *intf, char *pkt, uint32_t pkt_size, hdr_type_t hdr_type);
int traceoptions_handler(param_t *param, ser_buff_t *tlv_buf, op_mode enable_or_disable);

#endif
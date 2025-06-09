#include "graph.h"
#include "layer5.h"
#include "tcpconst.h"


static notif_chain_t layer2_proto_reg_db2 = {
    "L2 proto registration db",
    {NULL, NULL}
};

static notif_chain_t layer3_proto_reg_db2 = {
    "L3 proto registration db",
    {NULL, NULL}
};

static void layer5_invoke_app_cb(node_t *node, interface_t *recv_intf, char *l5_hdr, uint32_t pkt_size, uint32_t L5_protocol, uint32_t flags)
{
    pkt_notif_data_t pkt_notif_data;

    pkt_notif_data.recv_node = node;
    pkt_notif_data.recv_interface = recv_intf;
    pkt_notif_data.pkt = l5_hdr;
    pkt_notif_data.pkt_size = pkt_size;
    pkt_notif_data.flags = flags;
    pkt_notif_data.protocol_no = L5_protocol;

    nfc_invoke_notif_chain(&layer2_proto_reg_db2, (void *)&pkt_notif_data, sizeof(pkt_notif_data_t), (char *)&L5_protocol, sizeof(L5_protocol));
    nfc_invoke_notif_chain(&layer3_proto_reg_db2, (void *)&pkt_notif_data, sizeof(pkt_notif_data_t), (char *)&L5_protocol, sizeof(L5_protocol));
}


void promote_pkt_to_layer5(node_t *node, interface_t *recv_intf, char *l5_hdr, uint32_t pkt_size, uint32_t L5_protocol, uint32_t flags)
{
    switch(L5_protocol){
        case USERAPP1:
        {
            break;
        }
        default:
            layer5_invoke_app_cb(node, recv_intf, l5_hdr, pkt_size, L5_protocol, flags);
            ;
    }
}

void tcp_app_register_l2_protocol_interest(uint32_t L5_protocol, nfc_app_cb app_layer_cb)
{
	notif_chain_elem_t nfce_template;
	
	memset(&nfce_template, 0, sizeof(notif_chain_elem_t));

	memcpy(&nfce_template.key, (char *)&L5_protocol, sizeof(L5_protocol));

	nfce_template.key_size = sizeof(L5_protocol);
	nfce_template.is_key_set = TRUE;
	nfce_template.app_cb = app_layer_cb;
	init_glthread(&nfce_template.glue);

	nfc_register_notif_chain(&layer2_proto_reg_db2, &nfce_template);
}

void tcp_app_register_l3_protocol_interest(uint32_t L5_protocol, nfc_app_cb app_layer_cb)
{
	notif_chain_elem_t nfce_template;
	
	memset(&nfce_template, 0, sizeof(notif_chain_elem_t));

	memcpy(&nfce_template.key, (char *)&L5_protocol, sizeof(L5_protocol));

	nfce_template.key_size = sizeof(L5_protocol);
	nfce_template.is_key_set = TRUE;
	nfce_template.app_cb = app_layer_cb;
	init_glthread(&nfce_template.glue);

	nfc_register_notif_chain(&layer3_proto_reg_db2, &nfce_template);
}
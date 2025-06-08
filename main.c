#include <stdio.h>
#include "graph.h"
#include "net.h"
#include "CommandParser/libcli.h"
#include "comm.h"

extern graph_t *build_first_topo(void);
extern graph_t *build_simple_l2_switch_topo(void);
extern graph_t *build_square_topo(void);
extern graph_t *build_dualswitch_topo(void);
extern graph_t *build_linear_topo(void);
extern graph_t *linear_3_node_topo(void);
extern graph_t *L2_loop_topo(void);
extern void nw_init_cli(void);
extern void init_tcp_ip_stack(void);

graph_t *topo = NULL;

int main(int argc, char **argv)
{
    nw_init_cli();
    show_help_handler(0, 0, MODE_UNKNOWN);
    //topo = build_first_topo();
    //topo = build_dualswitch_topo();
    //topo = linear_3_node_topo(); // to test ping
    topo = build_square_topo();
    dump_nw_graph(topo);
    /*
    sleep(2);
    node_t *snode = get_node_by_node_name(topo, "R0_re");
    printf("Node address: %x\n", snode);
    interface_t *oif = get_node_intf_by_name(snode, "eth0/0");
    printf("Interface address: %x\n", oif);
    char msg[] = "Hello! How are you?";
    send_pkt_out(msg, strlen(msg), oif);
    */
    init_tcp_ip_stack();
    start_shell();
    return 0;
}
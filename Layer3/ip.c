#include "ip.h"

void initialize_ip_pkt(ip_hdr_t *ip_hdr)
{
    ip_hdr->version = 4; // IPv4 or IPv6
    ip_hdr->hdr_len = 5; // Represented in multiples of 4 Bytes we keep this currently fixed at 20 Bytes, as we will no use "Options" Bytes
    ip_hdr->tos = 0;

    ip_hdr->total_length = 0; /* Represented in multiples of 4 Bytes - To be filled by the caller*/

    /*Fragmentation related will not be used
     * int this course, initialize them all to zero*/
    ip_hdr->identification = 0; 
    ip_hdr->unused_flag = 0;
    ip_hdr->DF_flag = 1;
    ip_hdr->MORE_flag = 0;
    ip_hdr->frag_offset = 0;

    ip_hdr->ttl = 64; /*Let us use 64*/
    ip_hdr->protocol = 0; /*To be filled by the caller- 6 for TCP and 17 for UDP */
    ip_hdr->checksum = 0; /*Not used in this course*/
    ip_hdr->src_ip = 0; /*To be filled by the caller*/ 
    ip_hdr->dst_ip = 0; /*To be filled by the caller*/
}

        
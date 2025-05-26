#ifndef IP_H
#define IP_H
#include <stdint.h>
#include "../net.h"

#pragma pack(push,1)
typedef struct ip_hdr_{
    unsigned short int version :4;    /* IP Protocol version. For IPv4 its 4; For IPv6 its 6 */
    unsigned short int hdr_len :4 ;   /* Length of IP header in DWord */
    char tos;
    unsigned short total_length;      /* length  of hdr + payload */

    /* Fragmentation related */
    unsigned short int identification;
    unsigned short int unused_flag : 1;
    unsigned short int DF_flag : 1;
    unsigned short int MORE_flag : 1;
    unsigned long int frag_offset : 13;

    char ttl;
    char protocol;
    unsigned short int checksum;
    unsigned int src_ip;
    unsigned int dst_ip;
}ip_hdr_t;
#pragma pop

#define IP_HDR_LEN_IN_BYTES(ip_hdr_ptr)       (ip_hdr_ptr->hdr_len * 4U) // TBC: sizeof(ip_hdr_t)
#define IP_PKT_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) (ip_hdr_ptr->total_length * 4U)
#define IP_PKT_PAYLOAD_PTR(ip_hdr_ptr)        ((char *)(ip_hdr_ptr + IP_HDR_LEN_IN_BYTES(ip_hdr_ptr)))
#define IP_PKT_PAYLOAD_SIZE(ip_hdr_ptr)       (IP_PKT_TOTAL_LEN_IN_BYTES(ip_hdr_ptr) - IP_HDR_LEN_IN_BYTES(ip_hdr_ptr))


#endif //IP_H
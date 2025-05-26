#include "utils.h"
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>


void layer2_fill_with_broadcast_mac(char *mac_array)
{
    unsigned int i;
    for(i = 0; i < 6U; i++)
    {
        mac_array[i] = 0xFFU;
    }
}

/* apply mask on prefix and store result in str_prefix
   For eg : prefix = 122.1.1.1, mask 24, then str_prefix
   will store 122.1.1.0*/
void apply_mask(char *prefix, char mask, char *str_prefix){

    uint32_t binary_prefix = 0U;
    uint32_t subnet_mask = 0xFFFFFFFFUL;

    if(mask == 32U){
        strncpy(str_prefix, prefix, 16);
        str_prefix[15] = '\0';
        return;
    }

    /* convert given Ip address in to binary format */
    inet_pton(AF_INET, prefix, &binary_prefix);
    binary_prefix = htonl(binary_prefix);

    /* compute mask in binary format as well */
    subnet_mask = subnet_mask << (32 - mask);
    /*Perform logical AND to apply mask on IP address*/
    binary_prefix = binary_prefix & subnet_mask;

    /*Convert the Final IP into string format again*/
    binary_prefix = htonl(binary_prefix);
    inet_ntop(AF_INET, &binary_prefix, str_prefix, 16);
    str_prefix[15] = '\0';
}


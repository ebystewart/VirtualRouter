#ifndef UTILS_H
#define UITLS_H

/* bit operations */
#define SET_BIT(var, pos)       (var | (1U << pos))
#define IS_BIT_SET(var, pos)    ((var & (1U << pos)) == 1U)

#define IS_MAC_BROADCAST_ADDR(mac) \
       (mac[0] == 0xFF && mac[1] == 0xFF && mac[2] == 0xFF && \
        mac[3] == 0xFF && mac[4] == 0xFF && mac[5] == 0xFF)


void layer2_fill_with_broadcast_mac(char *mac_array);

void apply_mask(char *prefix, char mask, char *str_prefix);
#endif  //UTILS_H
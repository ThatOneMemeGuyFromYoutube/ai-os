#ifndef ASTEROS_ICMP_H
#define ASTEROS_ICMP_H
#include <stdint.h>
typedef enum {
    ASTEROS_ICMP_OK = 0,
    ASTEROS_ICMP_INVALID_ARGUMENT,
    ASTEROS_ICMP_ALREADY_INITIALIZED,
    ASTEROS_ICMP_NOT_INITIALIZED,
    ASTEROS_ICMP_HANDLER_REGISTRATION_FAILED
} asteros_icmp_result_t;
asteros_icmp_result_t asteros_icmp_init(void);
#endif

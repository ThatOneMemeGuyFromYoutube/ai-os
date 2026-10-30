#include "ps2_wait.h"

uint8_t asteros_ps2_wait_status(asteros_ps2_status_reader_t read_status,
                                void *context, uint8_t mask,
                                uint8_t expected, uint32_t attempts) {
    uint32_t attempt;

    if (!read_status || attempts == 0) return 0;
    expected = (uint8_t)(expected & mask);

    for (attempt = 0; attempt < attempts; attempt++) {
        if ((read_status(context) & mask) == expected) return 1;
    }
    return 0;
}

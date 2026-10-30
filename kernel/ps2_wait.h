#ifndef ASTEROS_PS2_WAIT_H
#define ASTEROS_PS2_WAIT_H

#include <stdint.h>

typedef uint8_t (*asteros_ps2_status_reader_t)(void *context);

/* Poll a controller status source for a masked value, stopping after a
   finite number of reads so absent or wedged PS/2 hardware cannot hang boot. */
uint8_t asteros_ps2_wait_status(asteros_ps2_status_reader_t read_status,
                                void *context, uint8_t mask,
                                uint8_t expected, uint32_t attempts);

#endif

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "ps2_wait.h"

typedef struct {
    const uint8_t *values;
    size_t length;
    size_t reads;
    uint8_t fallback;
} status_script_t;

static uint8_t scripted_status(void *context) {
    status_script_t *script = (status_script_t *)context;
    size_t index = script->reads++;
    if (index < script->length) return script->values[index];
    return script->fallback;
}

static void test_waits_until_mask_matches(void) {
    const uint8_t values[] = {0x02, 0x82, 0x00};
    status_script_t script = {values, sizeof values / sizeof values[0], 0, 0x02};

    assert(asteros_ps2_wait_status(scripted_status, &script, 0x02, 0x00, 5) == 1);
    assert(script.reads == 3);
}

static void test_mask_ignores_unrelated_status_bits(void) {
    const uint8_t values[] = {0x81};
    status_script_t script = {values, 1, 0, 0};

    assert(asteros_ps2_wait_status(scripted_status, &script, 0x01, 0xFF, 1) == 1);
    assert(script.reads == 1);
}

static void test_timeout_is_bounded(void) {
    const uint8_t values[] = {0x02, 0x22, 0x82};
    status_script_t script = {values, sizeof values / sizeof values[0], 0, 0x02};

    assert(asteros_ps2_wait_status(scripted_status, &script, 0x02, 0x00, 4) == 0);
    assert(script.reads == 4);
}

static void test_invalid_reader_and_zero_attempts_fail_without_reads(void) {
    status_script_t script = {NULL, 0, 0, 0};

    assert(asteros_ps2_wait_status(NULL, &script, 0x01, 0x01, 4) == 0);
    assert(asteros_ps2_wait_status(scripted_status, &script, 0x01, 0x01, 0) == 0);
    assert(script.reads == 0);
}

int main(void) {
    test_waits_until_mask_matches();
    test_mask_ignores_unrelated_status_bits();
    test_timeout_is_bounded();
    test_invalid_reader_and_zero_attempts_fail_without_reads();
    puts("PS/2 wait: tests passed");
    return 0;
}

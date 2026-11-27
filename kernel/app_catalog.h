#ifndef ASTEROS_APP_CATALOG_H
#define ASTEROS_APP_CATALOG_H
#include <stdint.h>
#include "desktop_input.h"
typedef struct {
    int8_t id;
    const char *name;
    const char *summary;
    const char *usage;
    const char *aliases;
    const char *limitations;
    const char *const *commands;
} asteros_app_info_t;
uint8_t asteros_app_catalog_count(void);
const asteros_app_info_t *asteros_app_catalog_at(uint8_t index);
const asteros_app_info_t *asteros_app_catalog_by_id(int8_t id);
const asteros_app_info_t *asteros_app_catalog_by_command(const char *command);
int8_t asteros_app_catalog_resolve_command(const char *command);
#endif

#ifndef ASTEROS_FS_H
#define ASTEROS_FS_H

#include <stdint.h>

/* Small volatile 8.3-name RAM filesystem. Contents are lost on reboot. */
#define ASTEROS_FS_MAX_FILES 8
#define ASTEROS_FS_NAME_MAX 12
#define ASTEROS_FS_FILE_MAX 1023

typedef enum {
    ASTEROS_FS_OK = 0,
    ASTEROS_FS_INVALID_ARGUMENT,
    ASTEROS_FS_INVALID_NAME,
    ASTEROS_FS_NOT_FOUND,
    ASTEROS_FS_NO_SPACE,
    ASTEROS_FS_TOO_LARGE,
    ASTEROS_FS_BUFFER_TOO_SMALL
} asteros_fs_status_t;

typedef struct {
    char name[ASTEROS_FS_NAME_MAX + 1u];
    uint16_t length;
} asteros_fs_info_t;

void asteros_fs_init(void);

/* Normalize an 8.3 filename to uppercase. Paths and directories are not
   supported; accepted characters are A-Z, 0-9, underscore, and hyphen. */
asteros_fs_status_t asteros_fs_normalize_name(
    const char *name, char normalized[ASTEROS_FS_NAME_MAX + 1u]);

/* Validate the complete request before replacing a slot. Data is copied into
   fixed storage; no heap allocation, disk persistence, or concurrent writers. */
asteros_fs_status_t asteros_fs_write(const char *name,
                                     const uint8_t *data, uint16_t length);

/* A short destination is rejected without a partial copy. required_length is
   set to the stored length whenever the file exists. */
asteros_fs_status_t asteros_fs_read(const char *name, uint8_t *destination,
                                    uint16_t capacity,
                                    uint16_t *required_length);
asteros_fs_status_t asteros_fs_delete(const char *name);

uint8_t asteros_fs_count(void);
/* Enumerate occupied slots in stable slot order, not directory sort order. */
asteros_fs_status_t asteros_fs_stat(uint8_t index, asteros_fs_info_t *info);
const char *asteros_fs_status_message(asteros_fs_status_t status);

#endif

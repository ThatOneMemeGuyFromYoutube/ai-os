#include "fs.h"

typedef struct {
    uint8_t used;
    char name[ASTEROS_FS_NAME_MAX + 1u];
    uint16_t length;
    uint8_t data[ASTEROS_FS_FILE_MAX];
} asteros_fs_file_t;

static asteros_fs_file_t files[ASTEROS_FS_MAX_FILES];

static char ascii_upper(char value) {
    if (value >= 'a' && value <= 'z')
        return (char)(value - ('a' - 'A'));
    return value;
}

static uint8_t name_equal(const char *left, const char *right) {
    uint8_t index = 0;
    while (left[index] && right[index] && left[index] == right[index])
        index++;
    return (uint8_t)(left[index] == right[index]);
}

static int8_t find_file(const char *normalized) {
    uint8_t index;
    for (index = 0; index < ASTEROS_FS_MAX_FILES; index++) {
        if (files[index].used && name_equal(files[index].name, normalized))
            return (int8_t)index;
    }
    return -1;
}

asteros_fs_status_t asteros_fs_normalize_name(
    const char *name, char normalized[ASTEROS_FS_NAME_MAX + 1u]) {
    uint16_t length = 0;
    uint8_t base_length = 0;
    uint8_t extension_length = 0;
    uint8_t has_extension = 0;
    uint16_t index;

    if (!name || !normalized) return ASTEROS_FS_INVALID_ARGUMENT;
    while (length <= ASTEROS_FS_NAME_MAX && name[length]) length++;
    if (length == 0 || length > ASTEROS_FS_NAME_MAX)
        return ASTEROS_FS_INVALID_NAME;

    for (index = 0; index < length; index++) {
        char value = ascii_upper(name[index]);
        if (value == '.') {
            if (has_extension || base_length == 0 || base_length > 8u)
                return ASTEROS_FS_INVALID_NAME;
            has_extension = 1;
        } else {
            uint8_t valid = (uint8_t)(
                (value >= 'A' && value <= 'Z') ||
                (value >= '0' && value <= '9') ||
                value == '_' || value == '-');
            if (!valid) return ASTEROS_FS_INVALID_NAME;
            if (has_extension) {
                if (++extension_length > 3u)
                    return ASTEROS_FS_INVALID_NAME;
            } else if (++base_length > 8u) {
                return ASTEROS_FS_INVALID_NAME;
            }
        }
        normalized[index] = value;
    }
    if (has_extension && extension_length == 0u)
        return ASTEROS_FS_INVALID_NAME;
    normalized[length] = '\0';
    return ASTEROS_FS_OK;
}

void asteros_fs_init(void) {
    uint8_t index;
    for (index = 0; index < ASTEROS_FS_MAX_FILES; index++) {
        files[index].used = 0;
        files[index].name[0] = '\0';
        files[index].length = 0;
    }
}

asteros_fs_status_t asteros_fs_write(const char *name,
                                     const uint8_t *data, uint16_t length) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    asteros_fs_status_t status;
    int8_t slot;
    uint16_t data_index;
    uint8_t index;

    if (length > ASTEROS_FS_FILE_MAX) return ASTEROS_FS_TOO_LARGE;
    if (length && !data) return ASTEROS_FS_INVALID_ARGUMENT;
    status = asteros_fs_normalize_name(name, normalized);
    if (status != ASTEROS_FS_OK) return status;

    slot = find_file(normalized);
    if (slot < 0) {
        for (index = 0; index < ASTEROS_FS_MAX_FILES; index++) {
            if (!files[index].used) {
                slot = (int8_t)index;
                break;
            }
        }
    }
    if (slot < 0) return ASTEROS_FS_NO_SPACE;

    /* All failure conditions have been checked before mutating the slot. */
    for (data_index = 0; data_index < length; data_index++)
        files[(uint8_t)slot].data[data_index] = data[data_index];
    for (index = 0; index <= ASTEROS_FS_NAME_MAX; index++)
        files[(uint8_t)slot].name[index] = normalized[index];
    files[(uint8_t)slot].length = length;
    files[(uint8_t)slot].used = 1;
    return ASTEROS_FS_OK;
}

asteros_fs_status_t asteros_fs_read(const char *name, uint8_t *destination,
                                    uint16_t capacity,
                                    uint16_t *required_length) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    asteros_fs_status_t status;
    int8_t slot;
    uint16_t index;
    uint16_t length;

    if (required_length) *required_length = 0;
    status = asteros_fs_normalize_name(name, normalized);
    if (status != ASTEROS_FS_OK) return status;
    slot = find_file(normalized);
    if (slot < 0) return ASTEROS_FS_NOT_FOUND;

    length = files[(uint8_t)slot].length;
    if (required_length) *required_length = length;
    if (capacity < length) return ASTEROS_FS_BUFFER_TOO_SMALL;
    if (length && !destination) return ASTEROS_FS_INVALID_ARGUMENT;
    for (index = 0; index < length; index++)
        destination[index] = files[(uint8_t)slot].data[index];
    return ASTEROS_FS_OK;
}

asteros_fs_status_t asteros_fs_delete(const char *name) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    asteros_fs_status_t status = asteros_fs_normalize_name(name, normalized);
    int8_t slot;

    if (status != ASTEROS_FS_OK) return status;
    slot = find_file(normalized);
    if (slot < 0) return ASTEROS_FS_NOT_FOUND;
    files[(uint8_t)slot].used = 0;
    files[(uint8_t)slot].name[0] = '\0';
    files[(uint8_t)slot].length = 0;
    return ASTEROS_FS_OK;
}

uint8_t asteros_fs_count(void) {
    uint8_t index;
    uint8_t count = 0;
    for (index = 0; index < ASTEROS_FS_MAX_FILES; index++)
        if (files[index].used) count++;
    return count;
}

asteros_fs_status_t asteros_fs_stat(uint8_t index, asteros_fs_info_t *info) {
    uint8_t slot;
    uint8_t found = 0;

    if (!info) return ASTEROS_FS_INVALID_ARGUMENT;
    for (slot = 0; slot < ASTEROS_FS_MAX_FILES; slot++) {
        if (!files[slot].used) continue;
        if (found++ != index) continue;
        for (uint8_t character = 0;
             character <= ASTEROS_FS_NAME_MAX; character++)
            info->name[character] = files[slot].name[character];
        info->length = files[slot].length;
        return ASTEROS_FS_OK;
    }
    return ASTEROS_FS_NOT_FOUND;
}

const char *asteros_fs_status_message(asteros_fs_status_t status) {
    switch (status) {
        case ASTEROS_FS_OK: return "OK";
        case ASTEROS_FS_INVALID_ARGUMENT: return "Invalid file operation";
        case ASTEROS_FS_INVALID_NAME: return "Invalid 8.3 filename";
        case ASTEROS_FS_NOT_FOUND: return "File not found";
        case ASTEROS_FS_NO_SPACE: return "RAM filesystem is full";
        case ASTEROS_FS_TOO_LARGE: return "File exceeds 1023 bytes";
        case ASTEROS_FS_BUFFER_TOO_SMALL: return "Destination buffer is too small";
        default: return "Unknown filesystem error";
    }
}

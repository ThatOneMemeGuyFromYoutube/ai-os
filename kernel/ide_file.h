#ifndef ASTEROS_IDE_FILE_H
#define ASTEROS_IDE_FILE_H

#include "editor.h"
#include "fs.h"

typedef enum {
    ASTEROS_IDE_FILE_OK = 0,
    ASTEROS_IDE_FILE_INVALID_ARGUMENT,
    ASTEROS_IDE_FILE_UNSAVED_CHANGES,
    ASTEROS_IDE_FILE_INVALID_TEXT,
    ASTEROS_IDE_FILE_FILESYSTEM_ERROR
} asteros_ide_file_status_t;

typedef struct {
    char name[ASTEROS_FS_NAME_MAX + 1u];
} asteros_ide_file_t;

void asteros_ide_file_init(asteros_ide_file_t *file, const char *initial_name);
asteros_ide_file_status_t asteros_ide_file_save(
    asteros_ide_file_t *file, asteros_editor_t *editor, const char *name,
    asteros_fs_status_t *filesystem_error);
asteros_ide_file_status_t asteros_ide_file_open(
    asteros_ide_file_t *file, asteros_editor_t *editor, const char *name,
    asteros_fs_status_t *filesystem_error);
const char *asteros_ide_file_status_message(
    asteros_ide_file_status_t status, asteros_fs_status_t filesystem_error);

#endif

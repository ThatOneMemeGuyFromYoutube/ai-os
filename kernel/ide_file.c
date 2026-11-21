#include "ide_file.h"

static void copy_name(char *destination, const char *source) {
    uint8_t index;
    for (index = 0; index <= ASTEROS_FS_NAME_MAX; index++)
        destination[index] = source[index];
}

void asteros_ide_file_init(asteros_ide_file_t *file, const char *initial_name) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    if (!file) return;
    if (asteros_fs_normalize_name(initial_name, normalized) == ASTEROS_FS_OK)
        copy_name(file->name, normalized);
    else
        file->name[0] = '\0';
}

asteros_ide_file_status_t asteros_ide_file_save(
    asteros_ide_file_t *file, asteros_editor_t *editor, const char *name,
    asteros_fs_status_t *filesystem_error) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    asteros_fs_status_t fs_status;

    if (filesystem_error) *filesystem_error = ASTEROS_FS_OK;
    if (!file || !editor || !name) return ASTEROS_IDE_FILE_INVALID_ARGUMENT;
    fs_status = asteros_fs_normalize_name(name, normalized);
    if (fs_status == ASTEROS_FS_OK)
        fs_status = asteros_fs_write(normalized,
            (const uint8_t *)asteros_editor_text(editor), editor->length);
    if (fs_status != ASTEROS_FS_OK) {
        if (filesystem_error) *filesystem_error = fs_status;
        return ASTEROS_IDE_FILE_FILESYSTEM_ERROR;
    }
    copy_name(file->name, normalized);
    asteros_editor_mark_clean(editor);
    return ASTEROS_IDE_FILE_OK;
}

asteros_ide_file_status_t asteros_ide_file_open(
    asteros_ide_file_t *file, asteros_editor_t *editor, const char *name,
    asteros_fs_status_t *filesystem_error) {
    uint8_t contents[ASTEROS_EDITOR_CAPACITY];
    uint16_t length = 0;
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    asteros_fs_status_t fs_status;

    if (filesystem_error) *filesystem_error = ASTEROS_FS_OK;
    if (!file || !editor || !name) return ASTEROS_IDE_FILE_INVALID_ARGUMENT;
    if (asteros_editor_is_dirty(editor))
        return ASTEROS_IDE_FILE_UNSAVED_CHANGES;

    fs_status = asteros_fs_normalize_name(name, normalized);
    if (fs_status == ASTEROS_FS_OK)
        fs_status = asteros_fs_read(normalized, contents,
            (uint16_t)(sizeof contents - 1u), &length);
    if (fs_status != ASTEROS_FS_OK) {
        if (filesystem_error) *filesystem_error = fs_status;
        return ASTEROS_IDE_FILE_FILESYSTEM_ERROR;
    }

    contents[length] = '\0';
    if (!asteros_editor_load(editor, (const char *)contents, length))
        return ASTEROS_IDE_FILE_INVALID_TEXT;
    copy_name(file->name, normalized);
    return ASTEROS_IDE_FILE_OK;
}

const char *asteros_ide_file_status_message(
    asteros_ide_file_status_t status, asteros_fs_status_t filesystem_error) {
    switch (status) {
        case ASTEROS_IDE_FILE_OK: return "OK";
        case ASTEROS_IDE_FILE_INVALID_ARGUMENT: return "Invalid IDE file operation";
        case ASTEROS_IDE_FILE_UNSAVED_CHANGES:
            return "Unsaved edits: save before opening another file";
        case ASTEROS_IDE_FILE_INVALID_TEXT:
            return "File contains unsupported editor text";
        case ASTEROS_IDE_FILE_FILESYSTEM_ERROR:
            return asteros_fs_status_message(filesystem_error);
        default:
            return "Unknown IDE file error";
    }
}

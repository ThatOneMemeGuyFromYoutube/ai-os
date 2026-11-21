#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ide_file.h"

static void test_save_as_and_reload(void) {
    asteros_editor_t editor;
    asteros_ide_file_t file;
    asteros_fs_status_t fs_error;

    asteros_fs_init();
    asteros_editor_init(&editor, "mov eax, 42\nexit");
    asteros_ide_file_init(&file, "hello.tl");
    assert(strcmp(file.name, "HELLO.TL") == 0);
    asteros_editor_move_down(&editor);
    asteros_editor_move_end(&editor);
    assert(asteros_editor_insert(&editor, '!'));
    assert(asteros_editor_is_dirty(&editor));

    assert(asteros_ide_file_save(&file, &editor, "demo.tl", &fs_error) ==
           ASTEROS_IDE_FILE_OK);
    assert(fs_error == ASTEROS_FS_OK);
    assert(strcmp(file.name, "DEMO.TL") == 0);
    assert(!asteros_editor_is_dirty(&editor));

    assert(asteros_editor_load(&editor, "discard this", 12));
    assert(asteros_ide_file_open(&file, &editor, "demo.tl", &fs_error) ==
           ASTEROS_IDE_FILE_OK);
    assert(strcmp(file.name, "DEMO.TL") == 0);
    assert(strcmp(asteros_editor_text(&editor), "mov eax, 42\nexit!") == 0);
    assert(!asteros_editor_is_dirty(&editor));
}

static void test_unsaved_open_is_rejected_without_mutation(void) {
    static const uint8_t other[] = "exit\n";
    asteros_editor_t editor;
    asteros_ide_file_t file;
    asteros_fs_status_t fs_error;

    asteros_fs_init();
    asteros_editor_init(&editor, "current");
    asteros_ide_file_init(&file, "CURRENT.TL");
    assert(asteros_fs_write("OTHER.TL", other, sizeof other - 1u) == ASTEROS_FS_OK);
    assert(asteros_editor_insert(&editor, '!'));

    assert(asteros_ide_file_open(&file, &editor, "OTHER.TL", &fs_error) ==
           ASTEROS_IDE_FILE_UNSAVED_CHANGES);
    assert(strcmp(file.name, "CURRENT.TL") == 0);
    assert(strcmp(asteros_editor_text(&editor), "!current") == 0);
    assert(asteros_editor_is_dirty(&editor));
    assert(strcmp(asteros_ide_file_status_message(
        ASTEROS_IDE_FILE_UNSAVED_CHANGES, ASTEROS_FS_OK),
        "Unsaved edits: save before opening another file") == 0);
}

static void test_errors_preserve_editor_and_filename(void) {
    asteros_editor_t editor;
    asteros_ide_file_t file;
    asteros_fs_status_t fs_error;

    asteros_fs_init();
    asteros_editor_init(&editor, "safe");
    asteros_ide_file_init(&file, "SAFE.TL");

    assert(asteros_ide_file_open(&file, &editor, "MISSING.TL", &fs_error) ==
           ASTEROS_IDE_FILE_FILESYSTEM_ERROR);
    assert(fs_error == ASTEROS_FS_NOT_FOUND);
    assert(strcmp(file.name, "SAFE.TL") == 0);
    assert(strcmp(asteros_editor_text(&editor), "safe") == 0);

    assert(asteros_ide_file_save(&file, &editor, "../BAD.TL", &fs_error) ==
           ASTEROS_IDE_FILE_FILESYSTEM_ERROR);
    assert(fs_error == ASTEROS_FS_INVALID_NAME);
    assert(strcmp(file.name, "SAFE.TL") == 0);
    assert(strcmp(asteros_editor_text(&editor), "safe") == 0);

    {
        static const uint8_t binary[] = { 'x', 0, 'y' };
        assert(asteros_fs_write("BINARY.TL", binary, sizeof binary) == ASTEROS_FS_OK);
    }
    assert(asteros_ide_file_open(&file, &editor, "BINARY.TL", &fs_error) ==
           ASTEROS_IDE_FILE_INVALID_TEXT);
    assert(strcmp(file.name, "SAFE.TL") == 0);
    assert(strcmp(asteros_editor_text(&editor), "safe") == 0);
}

int main(void) {
    test_save_as_and_reload();
    test_unsaved_open_is_rejected_without_mutation();
    test_errors_preserve_editor_and_filename();
    puts("ide-file: tests passed");
    return 0;
}

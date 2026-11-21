#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fs.h"

static void test_name_normalization(void) {
    char normalized[ASTEROS_FS_NAME_MAX + 1u];
    assert(asteros_fs_normalize_name("hello.tl", normalized) == ASTEROS_FS_OK);
    assert(strcmp(normalized, "HELLO.TL") == 0);
    assert(asteros_fs_normalize_name("README", normalized) == ASTEROS_FS_OK);
    assert(strcmp(normalized, "README") == 0);
    assert(asteros_fs_normalize_name("12345678.123", normalized) == ASTEROS_FS_OK);
    assert(asteros_fs_normalize_name("123456789.TL", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name("FILE.TOOL", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name("FILE.", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name(".TL", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name("A.B.C", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name("../A.TL", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name("", normalized) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_normalize_name(0, normalized) == ASTEROS_FS_INVALID_ARGUMENT);
}

static void test_create_replace_and_read(void) {
    static const uint8_t first[] = "mov eax, 1\nexit\n";
    static const uint8_t second[] = "mov eax, 42\nexit\n";
    uint8_t buffer[64];
    uint16_t length = 0;

    asteros_fs_init();
    assert(asteros_fs_count() == 0);
    assert(asteros_fs_write("hello.tl", first, sizeof first - 1u) == ASTEROS_FS_OK);
    assert(asteros_fs_count() == 1);
    assert(asteros_fs_read("HELLO.TL", buffer, sizeof buffer, &length) == ASTEROS_FS_OK);
    assert(length == sizeof first - 1u);
    assert(memcmp(buffer, first, length) == 0);

    assert(asteros_fs_write("HELLO.TL", second, sizeof second - 1u) == ASTEROS_FS_OK);
    assert(asteros_fs_count() == 1);
    assert(asteros_fs_read("hello.tl", buffer, sizeof buffer, &length) == ASTEROS_FS_OK);
    assert(length == sizeof second - 1u);
    assert(memcmp(buffer, second, length) == 0);
}

static void test_read_failures_are_non_destructive(void) {
    static const uint8_t contents[] = "123456";
    uint8_t buffer[8];
    uint16_t length = 0;

    memset(buffer, 0xA5, sizeof buffer);
    asteros_fs_init();
    assert(asteros_fs_write("SMALL.TL", contents, sizeof contents - 1u) == ASTEROS_FS_OK);
    assert(asteros_fs_read("SMALL.TL", buffer, 3, &length) == ASTEROS_FS_BUFFER_TOO_SMALL);
    assert(length == sizeof contents - 1u);
    for (uint8_t index = 0; index < sizeof buffer; index++)
        assert(buffer[index] == 0xA5);
    assert(asteros_fs_read("MISSING.TL", buffer, sizeof buffer, &length) == ASTEROS_FS_NOT_FOUND);
    assert(length == 0);
    assert(asteros_fs_read("SMALL.TL", 0, sizeof buffer, &length) == ASTEROS_FS_INVALID_ARGUMENT);
}

static void test_capacity_and_atomic_rejection(void) {
    uint8_t data[ASTEROS_FS_FILE_MAX + 1u];
    char name[13];
    memset(data, 0x5A, sizeof data);
    asteros_fs_init();

    for (uint8_t index = 0; index < ASTEROS_FS_MAX_FILES; index++) {
        name[0] = (char)('A' + index);
        name[1] = '\0';
        assert(asteros_fs_write(name, data, 1) == ASTEROS_FS_OK);
    }
    assert(asteros_fs_count() == ASTEROS_FS_MAX_FILES);
    assert(asteros_fs_write("NINTH.TL", data, 1) == ASTEROS_FS_NO_SPACE);
    assert(asteros_fs_write("A", data, sizeof data) == ASTEROS_FS_TOO_LARGE);
    assert(asteros_fs_write("BAD/NAME", data, 1) == ASTEROS_FS_INVALID_NAME);
    assert(asteros_fs_write("NEW.TL", 0, 1) == ASTEROS_FS_INVALID_ARGUMENT);
    assert(asteros_fs_count() == ASTEROS_FS_MAX_FILES);

    assert(asteros_fs_delete("D") == ASTEROS_FS_OK);
    assert(asteros_fs_write("NINTH.TL", data, 1) == ASTEROS_FS_OK);
    assert(asteros_fs_count() == ASTEROS_FS_MAX_FILES);
}

static void test_empty_files_stat_and_delete(void) {
    asteros_fs_info_t info;
    uint16_t length = 99;
    uint8_t byte = 0;

    asteros_fs_init();
    assert(asteros_fs_write("EMPTY", 0, 0) == ASTEROS_FS_OK);
    assert(asteros_fs_stat(0, &info) == ASTEROS_FS_OK);
    assert(strcmp(info.name, "EMPTY") == 0);
    assert(info.length == 0);
    assert(asteros_fs_stat(1, &info) == ASTEROS_FS_NOT_FOUND);
    assert(asteros_fs_stat(0, 0) == ASTEROS_FS_INVALID_ARGUMENT);
    assert(asteros_fs_read("EMPTY", &byte, 0, &length) == ASTEROS_FS_OK);
    assert(length == 0);
    assert(asteros_fs_delete("EMPTY") == ASTEROS_FS_OK);
    assert(asteros_fs_delete("EMPTY") == ASTEROS_FS_NOT_FOUND);
    assert(asteros_fs_count() == 0);
}

int main(void) {
    test_name_normalization();
    test_create_replace_and_read();
    test_read_failures_are_non_destructive();
    test_capacity_and_atomic_rejection();
    test_empty_files_stat_and_delete();
    puts("filesystem: tests passed");
    return 0;
}

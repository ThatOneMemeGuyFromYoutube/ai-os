#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_catalog.h"

static void test_catalog_is_complete_and_bounded(void) {
    uint8_t index;
    assert(asteros_app_catalog_count() == ASTEROS_APP_COUNT);
    for (index = 0; index < asteros_app_catalog_count(); index++) {
        const asteros_app_info_t *info = asteros_app_catalog_at(index);
        uint8_t other;
        assert(info && info->id >= ASTEROS_APP_TERMINAL && info->id < ASTEROS_APP_COUNT);
        assert(info->name && info->name[0]);
        assert(info->summary && info->summary[0]);
        assert(info->usage && info->usage[0]);
        assert(info->aliases && info->aliases[0]);
        assert(info->limitations && info->limitations[0]);
        assert(info->commands && info->commands[0]);
        assert(strlen(info->name) <= 11u);
        for (other=(uint8_t)(index+1u); other<asteros_app_catalog_count(); other++)
            assert(asteros_app_catalog_at(other)->id != info->id);
        assert(asteros_app_catalog_by_id(info->id) == info);
    }
    assert(asteros_app_catalog_at(ASTEROS_APP_COUNT) == 0);
    assert(asteros_app_catalog_by_id(-1) == 0);
    assert(asteros_app_catalog_by_id(99) == 0);
}
static void test_run_aliases_share_catalog_identity(void) {
    assert(asteros_app_catalog_resolve_command("terminal") == ASTEROS_APP_TERMINAL);
    assert(asteros_app_catalog_resolve_command(" TERM ") == ASTEROS_APP_TERMINAL);
    assert(asteros_app_catalog_resolve_command("console") == ASTEROS_APP_TERMINAL);
    assert(asteros_app_catalog_resolve_command("Files") == ASTEROS_APP_FILES);
    assert(asteros_app_catalog_resolve_command("computer") == ASTEROS_APP_FILES);
    assert(asteros_app_catalog_resolve_command("explorer") == ASTEROS_APP_FILES);
    assert(asteros_app_catalog_resolve_command("programs") == ASTEROS_APP_PROGRAMS);
    assert(asteros_app_catalog_resolve_command("apps") == ASTEROS_APP_PROGRAMS);
    assert(asteros_app_catalog_resolve_command("IDE") == ASTEROS_APP_IDE);
    assert(asteros_app_catalog_resolve_command("editor") == ASTEROS_APP_IDE);
    assert(asteros_app_catalog_resolve_command("settings") == ASTEROS_APP_SETTINGS);
    assert(asteros_app_catalog_resolve_command(" CONTROL PANEL ") == ASTEROS_APP_SETTINGS);
    assert(asteros_app_catalog_resolve_command("about") == ASTEROS_APP_ABOUT);
    assert(asteros_app_catalog_resolve_command("help") == ASTEROS_APP_ABOUT);
    assert(asteros_app_catalog_resolve_command("") == ASTEROS_HIT_NONE);
    assert(asteros_app_catalog_resolve_command("ide --run") == ASTEROS_HIT_NONE);
    assert(asteros_app_catalog_resolve_command("kernel") == ASTEROS_HIT_NONE);
    assert(asteros_app_catalog_resolve_command(0) == ASTEROS_HIT_NONE);
}
int main(void) {
    test_catalog_is_complete_and_bounded();
    test_run_aliases_share_catalog_identity();
    puts("application catalog: tests passed");
    return 0;
}

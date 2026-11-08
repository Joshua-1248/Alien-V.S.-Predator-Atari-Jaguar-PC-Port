#include <assert.h>
#include <stddef.h>
#include "computer.h"

/* The retail table is intentionally data-only and private to computer.c.
 * Include it here so the source-derived RDB addresses and authored page counts
 * remain regression locked without exposing test-only runtime APIs. */
#include "../src/game/computer_retail_tables.inc"

int main(void)
{
    assert(content_libtab.pages == 5u);
    assert(content_libtab.access9_alternate == &content_libtab_access9);
    assert(content_libtab_access9.pages == 6u);
    assert(content_libtab_access9.page_address[5] == 0x28530u);
    assert(content_comptab.pages == 6u);
    assert(content_comptab.page_address[0] == 0x27ad8u);
    assert(content_comptab.page_address[5] == 0x27f18u);
    assert(content_brieftab.pages == 3u);
    assert(content_brieftab.page_address[2] == 0x26a12u);
    assert(content_podtab.pages == 1u);
    assert(content_podtab.page_address[0] == 0x29272u);
    assert(content_comtab.allows_destruct == 1u);
    assert(sizeof(retail_level1_terminals)/sizeof(retail_level1_terminals[0]) == 8u);
    assert(sizeof(retail_level5_terminals)/sizeof(retail_level5_terminals[0]) == 11u);
    return 0;
}

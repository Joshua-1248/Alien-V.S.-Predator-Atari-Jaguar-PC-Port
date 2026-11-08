#include "local_mp_retail_maps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(X) do { if (!(X)) {fprintf(stderr,"FAIL %s at %d\n",#X,__LINE__);return 1;} } while (0)

int main(void)
{
    AvpMpRetailMaps store = {0}, bad_store = {0};
    AvpMpRetailMapSource sources[AVP_MP_RETAIL_LEVELS] = {{0}};
    uint8_t original[AVP_MP_RETAIL_LEVELS][8u + 12u*10u*8u];
    AvpMpRetailMap *ship_p1, *ship_p2, *pred_ship, *marine_ship;
    unsigned i, x, y;
    for (i = 0; i < AVP_MP_RETAIL_LEVELS; ++i) {
        uint8_t *m = original[i];
        memset(m, 0, sizeof(original[i]));
        m[1] = 12; m[3] = 10; m[5] = 2;
        for (y = 0; y < 10u; ++y) for (x = 0; x < 12u; ++x) {
            uint8_t *cell = m + 8u + (y*12u+x)*8u;
            cell[4] = (uint8_t)(i+1u);
            cell[5] = 1u;
        }
        sources[i].decoded = m;
        sources[i].decoded_size = sizeof(original[i]);
        sources[i].width = 12u;
        sources[i].height = 10u;
        sources[i].file_index = AVP_MP_RETAIL_MAZE_FIRST_FILE + i;
    }
    CHECK(avp_mp_retail_maps_init(&store, sources, AVP_MP_RETAIL_LEVELS));
    CHECK(!avp_mp_retail_maps_init(&store, sources, AVP_MP_RETAIL_LEVELS));
    ship_p1 = avp_mp_retail_maps_acquire(&store, 2, 7);
    ship_p2 = avp_mp_retail_maps_acquire(&store, 2, 7);
    pred_ship = avp_mp_retail_maps_acquire(&store, 2, 12);
    marine_ship = avp_mp_retail_maps_acquire(&store, 0, 7);
    CHECK(ship_p1 && ship_p2 && pred_ship && marine_ship);
    CHECK(ship_p1 == ship_p2 && ship_p1 != pred_ship && ship_p1 != marine_ship);
    CHECK(ship_p1->leases == 2 && pred_ship->leases == 1);
    CHECK(ship_p1->width == 12 && ship_p1->height == 10);
    CHECK(avp_mp_retail_map_cell(ship_p1, 3, 4));
    CHECK(!avp_mp_retail_map_cell(ship_p1, 12, 4));
    CHECK(!avp_mp_retail_map_cell(ship_p1, 3, 10));
    /* The per-world map changes, the two actors share it, other levels and
       campaigns and the caller's original decoded retail resource stay intact. */
    avp_mp_retail_map_cell(ship_p1, 3, 4)[2] = 0xA5;
    CHECK(avp_mp_retail_map_cell(ship_p2, 3, 4)[2] == 0xA5);
    CHECK(avp_mp_retail_map_cell(pred_ship, 3, 4)[2] == 0);
    CHECK(avp_mp_retail_map_cell(marine_ship, 3, 4)[2] == 0);
    CHECK(original[6][8u + (4u*12u+3u)*8u+2u] == 0);
    CHECK(!avp_mp_retail_maps_reset_campaign(&store, 2)); /* live worlds */
    CHECK(!avp_mp_retail_maps_destroy(&store));
    CHECK(!avp_mp_retail_maps_release(&store, 1, 7, ship_p1));
    CHECK(avp_mp_retail_maps_release(&store, 2, 7, ship_p1));
    CHECK(ship_p1->leases == 1);
    CHECK(avp_mp_retail_maps_release(&store, 2, 7, ship_p2));
    CHECK(!avp_mp_retail_maps_release(&store, 2, 7, ship_p2));
    /* The zero-player world remains modified on revisit: doors do not reset. */
    ship_p1 = avp_mp_retail_maps_acquire(&store, 2, 7);
    CHECK(ship_p1 && avp_mp_retail_map_cell(ship_p1, 3, 4)[2] == 0xA5);
    CHECK(avp_mp_retail_maps_release(&store, 2, 7, ship_p1));
    CHECK(avp_mp_retail_maps_release(&store, 2, 12, pred_ship));
    CHECK(avp_mp_retail_maps_reset_campaign(&store, 2));
    ship_p1 = avp_mp_retail_maps_acquire(&store, 2, 7);
    CHECK(ship_p1 && avp_mp_retail_map_cell(ship_p1, 3, 4)[2] == 0);
    CHECK(avp_mp_retail_maps_release(&store, 2, 7, ship_p1));
    CHECK(avp_mp_retail_maps_release(&store, 0, 7, marine_ship));
    CHECK(avp_mp_retail_maps_destroy(&store));
    /* Reject damaged, duplicate, missing and reordered FILES metadata. */
    sources[14].file_index = 87;
    CHECK(!avp_mp_retail_maps_init(&bad_store, sources, AVP_MP_RETAIL_LEVELS));
    CHECK(avp_mp_retail_maps_destroy(&bad_store));
    sources[14].file_index = 101;
    sources[2].decoded_size = 8u;
    CHECK(!avp_mp_retail_maps_init(&bad_store, sources, AVP_MP_RETAIL_LEVELS));
    sources[2].decoded_size = sizeof(original[2]);
    sources[1].width = 11; /* header disagrees with metadata */
    CHECK(!avp_mp_retail_maps_init(&bad_store, sources, AVP_MP_RETAIL_LEVELS));
    CHECK(avp_mp_retail_maps_destroy(&bad_store));
    puts("PASS: authentic-shaped FILES maps copied into independent persistent campaign-level contexts; shared same-level map; bounds and reset guards");
    return 0;
}

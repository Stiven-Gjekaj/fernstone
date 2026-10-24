#include "entity.h"
#include "test.h"

TEST(entity_adds_finds_and_removes) {
    static FernEntities t;
    FernEntity *e;
    fern_entities_clear(&t);
    e = fern_entity_add(&t, 1234, ENT_PLAYER);
    CHECK(e != NULL);
    fern_entity_place(e, 32 * 10, 32 * 64, -16);
    CHECK(fern_entity_get(&t, 1234) == e);
    CHECK(e->x == 10.0 && e->y == 64.0 && e->z == -0.5);
    CHECK(e->px == 10.0);
    fern_entity_move(e, 32, -32, 0);
    CHECK(e->x == 11.0 && e->y == 63.0);
    CHECK(e->px == 10.0);
    fern_entities_step(&t, 1.0);
    CHECK(e->px == 11.0);
    fern_entity_remove(&t, 1234);
    CHECK(fern_entity_get(&t, 1234) == NULL);
}

TEST(entity_keeps_two_ids_with_the_same_slot) {
    static FernEntities t;
    fern_entities_clear(&t);
    fern_entity_add(&t, 1, ENT_MOB);
    fern_entity_add(&t, 1 + FERN_ENTITY_MAX, ENT_MOB);
    fern_entity_remove(&t, 1);
    CHECK(fern_entity_get(&t, 1 + FERN_ENTITY_MAX) != NULL);
}

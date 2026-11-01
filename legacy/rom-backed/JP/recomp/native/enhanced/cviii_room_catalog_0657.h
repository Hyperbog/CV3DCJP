#ifndef CVIII_ROOM_CATALOG_0657_H
#define CVIII_ROOM_CATALOG_0657_H
#include <stdint.h>
#include <stddef.h>
/* Conditional physical Mapper24 PRG raw0A $92AE-$AFA3 room-data hierarchy.
 * Roots, records and widths are structural source facts, not object names. */
typedef struct {
    uint8_t stage, second, third, width;
    uint16_t root, end_exclusive;
} CviiiRoomLeaf0657;
typedef struct {
    uint16_t leaf_count, stages, tier2, cells4, records7, terminators7;
    CviiiRoomLeaf0657 leaves[135];
} CviiiRoomCatalog0657;
/* Return 1 only for a full source-bounded 15/64/135 hierarchy with all
 * 709 four-byte cells and 163 seven-byte records. -1 leaves output intact.
 * No copyright-bearing source payload is embedded in this module. */
int cviii_room_catalog_0657(const uint8_t *bank,size_t bank_len,CviiiRoomCatalog0657 *out);
/* Return 1 if indexed, 0 if selector not structurally valid, -1 for
 * invalid pointer. Non-success never modifies output. */
int cviii_room_catalog_find_0657(const CviiiRoomCatalog0657 *catalog,
    uint8_t stage,uint8_t second,uint8_t third,CviiiRoomLeaf0657 *out);
#endif

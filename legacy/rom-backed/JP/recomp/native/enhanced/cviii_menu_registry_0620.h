/* JP0620: game-owned menu setting registry, independent of NES host/UI.
 * Extensible in native code now; not a public data-mod loader yet. */
#ifndef CVIII_MENU_REGISTRY_0620_H
#define CVIII_MENU_REGISTRY_0620_H
#include <stdbool.h>
#include <stddef.h>
#define CVIII_MENU_MAX_ITEMS 32u
typedef int (*CviiiMenuGet)(void *user,int *out);
typedef int (*CviiiMenuSet)(void *user,int value);
typedef struct {
    const char *key, *section, *label, *description;
    int minimum, maximum, step;
    void *user;
    CviiiMenuGet get;
    CviiiMenuSet set;
} CviiiMenuSetting;
typedef struct {
    CviiiMenuSetting items[CVIII_MENU_MAX_ITEMS];
    size_t count;
    bool sealed;
} CviiiMenuRegistry;
bool cviii_menu_register(CviiiMenuRegistry *r,const CviiiMenuSetting *item);
void cviii_menu_seal(CviiiMenuRegistry *r);
const CviiiMenuSetting *cviii_menu_find(const CviiiMenuRegistry *r,
                                      const char *key);
int cviii_menu_get(const CviiiMenuRegistry *r,const char *key,int *value);
int cviii_menu_set(const CviiiMenuRegistry *r,const char *key,int value);
#endif

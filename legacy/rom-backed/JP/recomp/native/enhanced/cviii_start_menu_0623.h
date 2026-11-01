/* JP0623: an extensible game-owned Start/Pause action registry.
 * Engine owns SDL, save serialization, graphics and gameplay pause.
 * This controller manages only stable action IDs and modal navigation. */
#ifndef CVIII_START_MENU_0623_H
#define CVIII_START_MENU_0623_H
#include <stddef.h>
#define CVIII_START_MAX 32u
typedef enum {
    CVIII_START_NONE=0, CVIII_START_SAVE=1, CVIII_START_LOAD=2,
    CVIII_START_AUDIO=3, CVIII_START_QUIT=4, CVIII_START_EXTENSION=5
} CviiiStartAction;
typedef int (*CviiiStartEnabled)(void *context);
typedef struct {
    const char *id;
    const char *label;
    CviiiStartAction action;
    CviiiStartEnabled enabled;
    void *context;
} CviiiStartEntry;
typedef struct {
    CviiiStartEntry entries[CVIII_START_MAX];
    size_t count,selected;
    int open,quit_armed,sealed;
} CviiiStartMenu;
int cviii_start_register(CviiiStartMenu *menu,const CviiiStartEntry *entry);
int cviii_start_defaults(CviiiStartMenu *menu);
void cviii_start_seal(CviiiStartMenu *menu);
const CviiiStartEntry *cviii_start_find(const CviiiStartMenu *menu,const char *id);
int cviii_start_toggle(CviiiStartMenu *menu,int enhanced,int assets_ready);
void cviii_start_close(CviiiStartMenu *menu);
int cviii_start_move(CviiiStartMenu *menu,int direction);
CviiiStartAction cviii_start_accept(CviiiStartMenu *menu);
#endif

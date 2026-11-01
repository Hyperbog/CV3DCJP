/* JP0623: namespaced action registry, no platform, original ROM or SDL. */
#include "cviii_start_menu_0623.h"
#include <string.h>
static int valid_id(const char *id)
{
    size_t n;
    int dot=0;
    if(!id || !(n=strlen(id)) || n>95u || id[0]=='.'||id[n-1]=='.')
        return 0;
    for(size_t i=0;i<n;++i) {
        const char c=id[i];
        if(c=='.'){ if(i && id[i-1]=='.')return 0;dot=1; }
        else if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||
                  c=='_'||c=='-')) return 0;
    }
    return dot;
}
const CviiiStartEntry *cviii_start_find(const CviiiStartMenu *menu,const char *id)
{
    if(!menu||!id)return NULL;
    for(size_t i=0;i<menu->count;++i)
        if(!strcmp(menu->entries[i].id,id))return &menu->entries[i];
    return NULL;
}
int cviii_start_register(CviiiStartMenu *menu,const CviiiStartEntry *entry)
{
    if(!menu||!entry||menu->sealed||menu->count>=CVIII_START_MAX ||
       !valid_id(entry->id)||!entry->label||!*entry->label||
       entry->action<=CVIII_START_NONE||entry->action>CVIII_START_EXTENSION||
       cviii_start_find(menu,entry->id))return 0;
    /* Only builtin defaults may register protected core.* names. */
    if(!strncmp(entry->id,"core.",5) ||
       entry->action!=CVIII_START_EXTENSION)return 0;
    menu->entries[menu->count++]=*entry;
    return 1;
}
int cviii_start_defaults(CviiiStartMenu *menu)
{
    static const CviiiStartEntry defaults[]={
        {"core.save_state","SAVE STATE",CVIII_START_SAVE,NULL,NULL},
        {"core.load_state","LOAD STATE",CVIII_START_LOAD,NULL,NULL},
        {"core.audio","AUDIO",CVIII_START_AUDIO,NULL,NULL},
        {"core.quit","QUIT",CVIII_START_QUIT,NULL,NULL}
    };
    if(!menu||menu->count||menu->sealed)return 0;
    for(size_t i=0;i<sizeof defaults/sizeof defaults[0];++i)
        menu->entries[menu->count++]=defaults[i];
    return 1;
}
void cviii_start_seal(CviiiStartMenu *menu)
{
    if(menu) menu->sealed=1;
}
int cviii_start_toggle(CviiiStartMenu *menu,int enhanced,int assets_ready)
{
    if(!menu)return 0;
    if(menu->open){ cviii_start_close(menu); return 1; }
    if(!enhanced||!assets_ready||!menu->count)return 0;
    menu->selected=0;menu->quit_armed=0;menu->open=1;
    return 1;
}
void cviii_start_close(CviiiStartMenu *menu)
{
    if(menu){menu->open=0;menu->quit_armed=0;}
}
int cviii_start_move(CviiiStartMenu *menu,int direction)
{
    if(!menu||!menu->open||!menu->count||(direction!=-1&&direction!=1))
        return 0;
    menu->quit_armed=0;
    menu->selected=(menu->selected+(direction<0?menu->count-1u:1u))%
                   menu->count;
    return 1;
}
CviiiStartAction cviii_start_accept(CviiiStartMenu *menu)
{
    if(!menu||!menu->open||menu->selected>=menu->count)
        return CVIII_START_NONE;
    const CviiiStartEntry *entry=&menu->entries[menu->selected];
    if(entry->enabled && !entry->enabled(entry->context))return CVIII_START_NONE;
    if(entry->action==CVIII_START_QUIT&&!menu->quit_armed) {
        menu->quit_armed=1;
        return CVIII_START_NONE;
    }
    if(entry->action!=CVIII_START_QUIT)menu->quit_armed=0;
    return entry->action;
}

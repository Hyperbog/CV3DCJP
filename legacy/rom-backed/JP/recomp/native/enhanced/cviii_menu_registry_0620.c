/* JP0620: stable IDs and validated 1-unit menu edits; no external UI dep. */
#include "cviii_menu_registry_0620.h"
#include <string.h>
static bool valid_key(const char *key)
{
    if (!key || !*key || !strchr(key,'.') || strlen(key)>95u) return false;
    static const char *const reserved[]={
        "cyc.","audio.","display.","graphics.","system."
    };
    for(size_t i=0;i<sizeof reserved/sizeof reserved[0];++i)
        if(!strncmp(key,reserved[i],strlen(reserved[i])))return false;
    for(const char *s=key;*s;++s) {
        if (!((*s>='a' && *s<='z')||(*s>='0' && *s<='9')||
              *s=='.'||*s=='_'||*s=='-'))return false;
    }
    return true;
}
const CviiiMenuSetting *cviii_menu_find(const CviiiMenuRegistry *r,
                                       const char *key)
{
    if(!r||!key)return NULL;
    for(size_t i=0;i<r->count;++i)
        if(!strcmp(r->items[i].key,key))return &r->items[i];
    return NULL;
}
bool cviii_menu_register(CviiiMenuRegistry *r,const CviiiMenuSetting *it)
{
    if(!r||!it||r->sealed||r->count>=CVIII_MENU_MAX_ITEMS||
       !valid_key(it->key)||!it->section||!*it->section||
       !it->label||!*it->label||!it->description||
       !it->get||!it->set||it->minimum>it->maximum||
       it->step<=0||it->step>it->maximum-it->minimum||
       cviii_menu_find(r,it->key))return false;
    r->items[r->count++]=*it;
    return true;
}
void cviii_menu_seal(CviiiMenuRegistry *r){if(r)r->sealed=true;}
int cviii_menu_get(const CviiiMenuRegistry *r,const char *key,int *value)
{
    const CviiiMenuSetting *it=cviii_menu_find(r,key);
    if(!it||!value||!it->get(it->user,value))return 0;
    return *value>=it->minimum && *value<=it->maximum;
}
int cviii_menu_set(const CviiiMenuRegistry *r,const char *key,int value)
{
    const CviiiMenuSetting *it=cviii_menu_find(r,key);
    if(!it||value<it->minimum||value>it->maximum||
       (value-it->minimum)%it->step!=0)return 0;
    return it->set(it->user,value);
}

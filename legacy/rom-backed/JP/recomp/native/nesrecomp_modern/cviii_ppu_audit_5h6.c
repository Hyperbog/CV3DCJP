#include "cviii_ppu_audit_5h6.h"
#include <inttypes.h>
#include <string.h>
static const int rows[]={64,80,96,112,119,120,121,128,144,160,176,192,208,224};
static int cyc_delta(int a,int b){int d=(a-b)%480;return d<0?d+480:d;}
bool cviii_ppu_probe_5h6(const CviiiPpuProbe5H6 *p,CviiiPpuSummary5H6 *out)
{
    if(!p||!out||!p->scroll_x||!p->scroll_y||!p->bg_table||!p->mask)return false;
    CviiiPpuSummary5H6 r={0};
    int lastx=0,lasty=0,lastrow=0;unsigned lastbg=0,lastmask=0;
    for(unsigned i=0;i<sizeof rows/sizeof rows[0];i++){
        int row=rows[i],x=p->scroll_x(row),y=p->scroll_y(row);
        unsigned bg=p->bg_table(row),mask=p->mask(row);
        if(x<0||x>=512||y<0||y>=480||(bg!=0u&&bg!=0x1000u))return false;
        if(i){
            r.horizontal_changes+=(x!=lastx);
            r.vertical_phase_breaks+=(cyc_delta(y,lasty)!=row-lastrow);
            r.pattern_changes+=(bg!=lastbg);
            r.mask_changes+=(mask!=lastmask);
        }
        if(row==119){r.x119=x;r.y119=y;r.bg119=bg;r.mask119=mask;}
        if(row==120){r.x120=x;r.y120=y;r.bg120=bg;r.mask120=mask;}
        if(row==121){r.x121=x;r.y121=y;}
        lastx=x;lasty=y;lastrow=row;lastbg=bg;lastmask=mask;
    }
    *out=r;return true;
}
bool cviii_ppu_open_5h6(CviiiPpuAudit5H6 *t,const char *path)
{
    if(!t||t->file||!path||!*path)return false;
    FILE *f=fopen(path,"w");if(!f)return false;
    if(fputs("frame,stage,area,room,gate,qualified,x119,x120,x121,y119,y120,y121,bg119,bg120,mask119,mask120,x_changes,y_phase_breaks,bg_changes,mask_changes,chr_end_frame_valid,chr_end_r4,chr_end_r5,chr_end_r6,chr_end_r7\n",f)==EOF){fclose(f);return false;}
    memset(t,0,sizeof *t);t->file=f;return true;
}
void cviii_ppu_close_5h6(CviiiPpuAudit5H6 *t){if(!t)return;if(t->file)fclose(t->file);t->file=NULL;}
bool cviii_ppu_append_5h6(CviiiPpuAudit5H6 *t,uint64_t frame,
    unsigned stage,unsigned area,unsigned room,unsigned gate,unsigned qualified,
    const CviiiPpuSummary5H6 *p,const uint8_t banks[4],bool banks_known)
{
    if(!t||!t->file||!p||stage>=15||qualified>1||
       (t->primed&&frame<=t->last_frame)||(banks_known&&!banks))return false;
    int b[4]={-1,-1,-1,-1};
    if(banks_known)for(unsigned i=0;i<4;i++)b[i]=banks[i];
    if(fprintf(t->file,"%" PRIu64 ",%u,%u,%u,%u,%u,%d,%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%d,%d\n",
        frame,stage,area,room,gate,qualified,p->x119,p->x120,p->x121,
        p->y119,p->y120,p->y121,p->bg119,p->bg120,p->mask119,p->mask120,
        p->horizontal_changes,p->vertical_phase_breaks,p->pattern_changes,
        p->mask_changes,banks_known?1u:0u,b[0],b[1],b[2],b[3])<0)return false;
    t->primed=true;t->last_frame=frame;
    if(p->vertical_phase_breaks||p->pattern_changes||p->mask_changes)fflush(t->file);
    return true;
}
unsigned cviii_ppu_camera_tops_5h6(unsigned stage,unsigned area,unsigned room,
                                  int y120,unsigned out[8])
{
    if(!out)return 0;
    const unsigned regular[]={48u,32u,0u};
    for(unsigned i=0;i<3;i++)out[i]=regular[i];
    if(stage==0&&area==1&&room==0&&y120==119){
        /* One-pixel discrepancy at eight observed frames; no global shift. */
        const unsigned extra[]={47u,49u,31u,33u,1u};
        for(unsigned i=0;i<5;i++)out[i+3]=extra[i];
        return 8;
    }
    return 3;
}

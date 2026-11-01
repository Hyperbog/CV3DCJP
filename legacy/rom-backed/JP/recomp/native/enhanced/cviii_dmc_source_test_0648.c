/* JP0648 synthetic-only original DMC $6F..$7D event-boundary test.
 * Force assert side effects to run in Windows CMake Release as well. */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_dmc_source_0648.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint8_t fixture[CVIII_DMC_TABLE_BYTES];
static void check_writes(const CviiiDmcPlan *p,unsigned index)
{
    assert(p->accepted && p->write_count==6u);
    assert(p->writes[0].address==0x4015u && p->writes[0].value==0x0Fu);
    const uint16_t addresses[]={0x4010u,0x4011u,0x4012u,0x4013u};
    for(unsigned k=0;k<4u;++k) {
        assert(p->writes[k+1u].address==addresses[k]);
        assert(p->writes[k+1u].value==fixture[index*4u+k]);
    }
    assert(p->writes[5].address==0x4015u && p->writes[5].value==0x1Fu);
}
int main(void)
{
    for(unsigned i=0;i<CVIII_DMC_PROGRAMS;++i) {
        fixture[i*4u]=(uint8_t)(0x0Cu+(i%4u));
        fixture[i*4u+1u]=(uint8_t)i;
        fixture[i*4u+2u]=(uint8_t)(0x20u+i);
        fixture[i*4u+3u]=(uint8_t)(0x30u+i);
    }
    unsigned accepted=0,denied=0,counter=0;
    for(unsigned index=0;index<CVIII_DMC_PROGRAMS;++index)
        for(unsigned old=0;old<256u;++old) {
            CviiiDmcPlan p;
            memset(&p,0xAA,sizeof p);
            assert(cviii_dmc_plan_0648(fixture,sizeof fixture,0x6Fu+index,old,&p));
            assert(p.command==0x6Fu+index && p.index==index);
            assert(p.counter_increment==(index>=13u));
            assert(p.priority_after==(old>=5u && index<old ? old : index));
            if(index>=13u)++counter;
            if(old>=5u && index<old) {
                ++denied;
                assert(!p.accepted && p.write_count==0u);
                for(unsigned k=0;k<CVIII_DMC_WRITES;++k)
                    assert(p.writes[k].address==0u && p.writes[k].value==0u);
            } else {
                ++accepted;
                check_writes(&p,index);
            }
        }
    assert(accepted+denied==3840u && counter==512u);
    assert(denied>accepted); /* deliberate high priorities exercise reject */
    CviiiDmcPlan untouched;
    memset(&untouched,0xA5,sizeof untouched);
    CviiiDmcPlan original=untouched;
    for(unsigned bad=0;bad<5u;++bad) {
        const unsigned command=bad==0?0x6Eu:bad==1?0x7Eu:0x6Fu;
        const unsigned priority=bad==2?256u:0u;
        const size_t size=bad==3?59u:60u;
        const uint8_t *table=bad==4?NULL:fixture;
        assert(!cviii_dmc_plan_0648(table,size,command,priority,&untouched));
        assert(!memcmp(&untouched,&original,sizeof original));
    }
    assert(!cviii_dmc_plan_0648(fixture,60,0x6F,0,NULL));
    assert(!cviii_dmc_plan_0648(fixture,61,0x6F,0,&untouched));
    assert(!cviii_dmc_plan_0648(fixture,0,0x6F,0,&untouched));
    assert(!memcmp(&untouched,&original,sizeof original));
    puts("PASS JP0648 original DMC event planner: 15 programs x 256 priorities, six exact writes, protected unsupported indices");
    return 0;
}

/* JP0656: ROM-free exhaustive synthetic sound byte control audit.
 * Original opcode execution is independently tested against genuine
 * source with jp_phrase_control_0656.py --rom ... --native-lib .... */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "cviii_phrase_control_0656.h"
#include <assert.h>
#include <string.h>
static uint8_t bank[16384], ram[2048];
int main(void) {
    unsigned checked=0, matched=0, rewound=0;
    for (unsigned token=0xFBu; token<=0xFFu; ++token) {
        for (unsigned cursor=0;cursor<256u;++cursor) {
            for (unsigned counter=0;counter<256u;++counter) {
                CviiiPhraseControl0656 out;
                unsigned source=0xA200u;
                unsigned ptr=(source-cursor)&65535u;
                unsigned paramptr=(ptr+((cursor+1u)&255u))&65535u;
                uint8_t parameter=(uint8_t)((cursor ^ counter)&255u);
                unsigned x=3u, oldstart=(cursor*5u+33u)&255u;
                unsigned expectedkind;
                unsigned predictedcounter, predictedcursor, predictedstart;
                unsigned predictedhigh, predicteda, predicted_y, predicted_c, predicted_n, predicted_z;
                unsigned incoming_v=(token+cursor+counter)&1u;
                memset(ram,0,sizeof ram);
                memset(bank,0,sizeof bank);
                ram[0xE2]=(uint8_t)ptr;ram[0xE3]=(uint8_t)(ptr>>8);
                ram[0x06AA+x]=(uint8_t)cursor;
                ram[0x06A0+x]=(uint8_t)oldstart;
                ram[0x06AF+x]=(uint8_t)counter;
                ram[0x03F2+x]=0x7Cu;
                bank[source-0x8000u]=(uint8_t)token;
                bank[paramptr-0x8000u]=parameter;
                /* When cursor==255, parameter is elsewhere. Otherwise
                 * source+1. The parameter write never overlaps token. */
                assert(paramptr!=source);
                assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,
                       (uint8_t)x,(uint8_t)incoming_v,&out)==1);
                predictedcounter=counter;predictedcursor=cursor;
                predictedstart=oldstart;predictedhigh=0x7Cu;
                predicted_y=cursor; predicteda=0;
                predicted_c=0;predicted_z=0;predicted_n=0;
                if(token<=0xFDu) {
                    expectedkind=1;
                    predictedcursor=(cursor+1u)&255u;
                    predictedstart=predictedcursor;
                    predicteda=predictedcursor;
                    predicted_z=(predicteda==0u);
                    predicted_n=(predicteda>>7u);
                    assert(out.write_cursor==1 && out.write_repeat_start==1);
                    assert(!out.write_repeat_counter && !out.write_high_nibble);
                } else if(token==0xFEu) {
                    predicted_y=(cursor+1u)&255u;
                    predictedcounter=(counter+1u)&255u;
                    predicted_c=(parameter>=predictedcounter);
                    if(parameter==predictedcounter) {
                        ++matched;expectedkind=2;
                        predictedcounter=0;predictedstart=0;
                        predictedcursor=(cursor+2u)&255u;
                        predicteda=0;
                        predicted_n=predictedcursor>>7u;
                        predicted_z=(predictedcursor==0);
                        assert(out.write_cursor && out.write_repeat_start && out.write_repeat_counter);
                    } else {
                        ++rewound;expectedkind=3;
                        predictedcursor=oldstart;
                        predicteda=oldstart;
                        predicted_n=predicteda>>7u;
                        predicted_z=(predicteda==0);
                        assert(out.write_cursor && !out.write_repeat_start && out.write_repeat_counter);
                    }
                    assert(out.parameter==parameter && !out.write_high_nibble);
                } else {
                    expectedkind=4;
                    predicteda=255;predicted_c=1;predicted_n=1;predicted_z=0;
                    predictedhigh=255;
                    assert(out.write_high_nibble && !out.write_cursor &&
                           !out.write_repeat_start && !out.write_repeat_counter);
                }
                assert(out.kind==expectedkind && out.token==token);
                assert(out.x==x && out.y==predicted_y && out.a==predicteda);
                assert(out.carry==predicted_c && out.negative==predicted_n &&
                       out.zero==predicted_z && out.overflow==incoming_v);
                assert(out.cursor==predictedcursor && out.repeat_start==predictedstart);
                assert(out.repeat_counter==predictedcounter && out.high_nibble==predictedhigh);
                assert(out.token_cpu==source);
                assert(out.next_pc==(token==0xFF?0x8DCC:0x8D06));
                ++checked;
            }
        }
    }
    assert(checked==327680 && matched>0 && rewound>0);
    {
        CviiiPhraseControl0656 out,old;
        memset(&out,0x5a,sizeof out);old=out;
        memset(ram,0,sizeof ram);memset(bank,0,sizeof bank);
        ram[0xE2]=0;ram[0xE3]=0xA2;
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,0,&out)==0);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(bank,sizeof bank-1,ram,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram-1,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(NULL,sizeof bank,ram,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(bank,sizeof bank,NULL,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,2,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,0,NULL)==-1);
        ram[0xE3]=0x7f;
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        ram[0xE3]=0xc0;
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
        ram[0xE3]=0xbf;ram[0xE2]=0xff;ram[0x06AA]=0;
        bank[0x3fff]=0xfe;
        assert(cviii_phrase_control_0656(bank,sizeof bank,ram,sizeof ram,0,0,&out)==-1);
        assert(!memcmp(&old,&out,sizeof out));
    }
    return 0;
}

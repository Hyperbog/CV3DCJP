/* JP0610: 2A03/VRC6 register ownership evidence, private session only.
 * A rapid rewrite is a register-level observation, NOT a proof that the
 * game cancelled an effect or that this register belongs to music or SFX.
 */
#include "cviii_audio_bus_0610.h"
#include <inttypes.h>
#include <string.h>

static CviiiAudioBus *s_bus;
typedef struct { unsigned id; const char *chip, *voice; } Channel;
static Channel classify(uint16_t addr)
{
    if (addr >= 0x4000 && addr <= 0x4013) {
        unsigned n = (addr-0x4000)/4;
        static const char *const names[5] = {
            "pulse1","pulse2","triangle","noise","dmc"
        };
        Channel c={addr-0x4000u,"2a03",names[n]};
        return c;
    }
    if (addr == 0x4015 || addr == 0x4017) {
        Channel c={20u+(addr==0x4017),"2a03","control"};return c;
    }
    unsigned region=addr>>12, reg=addr&15u;
    if (region>=9 && region<=11 && reg <= (region==9 ? 3u : 2u)) {
        static const char *const names[3]={"pulse1","pulse2","saw"};
        Channel c={22u+(region-9u)*4u+reg,"vrc6",
                   region==9 && reg==3 ? "control" : names[region-9u]};
        return c;
    }
    Channel bad={40u,NULL,NULL};
    return bad;
}
int cviii_audio_bus_open(CviiiAudioBus *bus, const char *path)
{
    if (!bus || !path || !*path) return 0;
    if (s_bus) return 0;
    FILE *output=fopen(path,"w");
    if (!output) return 0;
    memset(bus,0,sizeof(*bus));
    bus->output=output;
    s_bus=bus;
    fputs("cycle,chip,voice,register,value,previous,delta_cycles,rapid_rewrite\n",
          output);
    return 1;
}
void cviii_audio_bus_close(CviiiAudioBus *bus)
{
    if(!bus)return;
    if(s_bus==bus)s_bus=NULL;
    if(bus->output)fclose(bus->output);
    bus->output=NULL;
}
void cviii_audio_bus_observe(uint64_t cycle,uint16_t addr,uint8_t value)
{
    CviiiAudioBus *bus=s_bus;
    if(!bus || !bus->output)return;
    Channel channel=classify(addr);
    if(channel.id >= 40u)return;
    if(bus->writes >= UINT64_C(1000000)){++bus->dropped;return;}
    unsigned id=channel.id;
    uint64_t delta=bus->seen[id] && cycle>=bus->last_cycle[id]
                    ? cycle-bus->last_cycle[id] : 0;
    int rapid=(bus->seen[id] && cycle>=bus->last_cycle[id]
               && delta<=6000u && bus->last_value[id]!=value);
    if(rapid)++bus->rapid_rewrites;
    fprintf(bus->output,"%" PRIu64 ",%s,%s,%04X,%02X,%02X,%" PRIu64 ",%d\n",
            cycle,channel.chip,channel.voice,(unsigned)addr,(unsigned)value,
            bus->seen[id] ? (unsigned)bus->last_value[id] : 0u,
            delta,rapid);
    bus->seen[id]=1;
    bus->last_value[id]=value;
    bus->last_cycle[id]=cycle;
    ++bus->writes;
    if((bus->writes&127u)==0u)fflush(bus->output);
}

/* Single canonical C11 translation unit: collision routines from JP ROM.
 * This file consolidates v2 FCDE, v3/v9 caller continuations, v4 dependencies,
 * v5 E648/bank7, and v6/v7 external routines.
 * Instruction semantic equivalence is the goal; no synthetic gameplay rules.
 * The original CPU/mapper/interrupt cycle behavior is NOT yet replaced.
 */
#include "Collision.h"
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/* ========== BOUNDED TRANSLATION: cviii_collision_fcde_native.h ========== */
/* Japanese Akumajou Densetsu, fixed PRG bank 15: $FC7D-$FD4C.
 * Readable 6502-to-C translation of the $FCDE query, not an instruction
 * interpreter. RAM is 2 KB NES internal RAM, ROM is authentic fixed 16 KB.
 * Does NOT replace upstream movement, damage, or caller-side gameplay.
 * 2A03 arithmetic is binary regardless of the decimal flag.
 */

typedef struct {
    uint8_t a, x, y, sp, p;
    unsigned instructions; unsigned cpu_cycles;
    uint16_t exit_pc; /* Address of original RTS, not the caller's PC. */
} CviiiCollisionNativeResult;

static void c3_nz(uint8_t *p, uint8_t v) {
    *p=(uint8_t)((*p & 0x7du) | (v==0 ? 0x02u:0u) | (v&0x80u));
}
static uint8_t c3_add(uint8_t a,uint8_t b,uint8_t *p) {
    unsigned q=(unsigned)a+b+(*p&1u);
    uint8_t r=(uint8_t)q;
    *p=(uint8_t)((*p&~0x41u)|(q>255u?1u:0u)|
          ((~(a^b)&(a^r)&0x80u)?0x40u:0u));
    c3_nz(p,r);return r;
}
static uint8_t c3_sub(uint8_t a,uint8_t b,uint8_t *p) {
    int q=(int)a-b-(1-(*p&1u));
    uint8_t r=(uint8_t)q;
    *p=(uint8_t)((*p&~0x41u)|(q>=0?1u:0u)|
          (((a^b)&(a^r)&0x80u)?0x40u:0u));
    c3_nz(p,r);return r;
}
static void c3_cmp(uint8_t a,uint8_t b,uint8_t *p) {
    *p=(uint8_t)((*p&~1u)|(a>=b?1u:0u));
    c3_nz(p,(uint8_t)(a-b));
}
static uint8_t c3_lsr(uint8_t v,uint8_t *p) {
    *p=(uint8_t)((*p&~1u)|(v&1u));
    v>>=1;c3_nz(p,v);return v;
}
static uint64_t c3_fnv64(const uint8_t *data, size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;++i)h=(h^data[i])*UINT64_C(1099511628211);
    return h;
}
static int cviii_collision_fcde_native(const uint8_t fixed_bank[16384],
        uint8_t ram[2048],uint8_t initial_a,uint8_t x,uint8_t initial_y,
        uint8_t initial_sp,uint8_t initial_p,CviiiCollisionNativeResult *out) {
    if(!fixed_bank||!ram||!out)return -1;
    /* The complete physical fixed bank fingerprint prevents wrong-version use. */
    if(c3_fnv64(fixed_bank,16384)!=UINT64_C(0x0ba6968ecce23e07))return -2;
    uint8_t a=initial_a,y=initial_y,sp=initial_sp,p=initial_p;
    unsigned steps=0,cycles=0;
#define T(N) do {steps++; cycles+=(unsigned)(N);}while(0)
    uint16_t exit_pc=0;
    /* $FCDE: PHA; LDA $65; BMI $FC80. The PHA writes to live stack RAM. */
    ram[0x100u+sp]=a;sp--;T(3);
    a=ram[0x65];c3_nz(&p,a);T(3);
    T(2u+((p&0x80u)!=0)); /* BMI */
    if(p&0x80u) {
        /* Vertical collision address computation, $FC80-$FCDC. */
        sp++;a=ram[0x100u+sp];c3_nz(&p,a);T(4);
        p&=(uint8_t)~1u;T(2);
        a=c3_add(a,ram[0x438],&p);T(4);
        y=a;c3_nz(&p,y);T(2);
        ram[0x13]=a;T(3);
        a=x;c3_nz(&p,a);T(2);
        p&=(uint8_t)~1u;T(2);
        a=c3_add(a,ram[0x41c],&p);T(4);
        c3_cmp(a,ram[0xcd],&p);T(3);
        T(2u+(!(p&1u))); /* BCC $FC7D */
        if(!(p&1u)) {
            a=0;c3_nz(&p,a);T(2);exit_pc=0xFC7F;
            goto done;
        }
        p|=1u;T(2);
        a=c3_sub(a,0x33u,&p);T(2);
        ram[0x11]=a;T(3);
        T(2u+((p&1u)!=0)); /* BCS $FC9C */
        if(p&1u) { a=0;c3_nz(&p,a);T(2); }
        else { a=0xffu;c3_nz(&p,a);T(2);T(3); /* BNE $FC9E */ }
        ram[0x13]=a;T(3);
        ram[0x10]=y;T(3);
        a=y;c3_nz(&p,a);T(2);
        for(unsigned i=0;i<5;++i){a=c3_lsr(a,&p);T(2);}
        ram[0x12]=a;T(3);
        a=ram[0x13];c3_nz(&p,a);T(3);
        T(2u+((p&0x80u)!=0)); /* BMI $FCBB */
        if(p&0x80u){
            p&=(uint8_t)~1u;T(2);
            a=ram[0x53];c3_nz(&p,a);T(3);
            a=c3_add(a,ram[0x11],&p);T(3);
            T(2u+((p&1u)!=0)); /* BCS $FCC8 */
            if(!(p&1u)){
                p|=1u;T(2);
                a=c3_sub(a,0x10u,&p);T(2);
                T(3); /* JMP $FCCF */
                goto vertical_quantize;
            }
        } else {
            p&=(uint8_t)~1u;T(2);
            a=ram[0x53];c3_nz(&p,a);T(3);
            a=c3_add(a,ram[0x11],&p);T(3);
            T(2u+(!(p&1u))); /* BCC $FCC8 */
            if(p&1u){
                p&=(uint8_t)~1u;T(2);
                a=c3_add(a,0x10u,&p);T(2);
                T(3); /* JMP $FCCF */
                goto vertical_quantize;
            }
        }
        c3_cmp(a,0xf0u,&p);T(2);
        T(2u+(!(p&1u))); /* BCC $FCCF */
        if(p&1u){p&=(uint8_t)~1u;T(2);a=c3_add(a,0x10u,&p);T(2);}
vertical_quantize:
        a&=0xf0u;c3_nz(&p,a);T(2);
        a=c3_lsr(a,&p);T(2);
        p&=(uint8_t)~1u;T(2);
        a=c3_add(a,ram[0x12],&p);T(3);
        y=a;c3_nz(&p,y);T(2);
        a=ram[0x10];c3_nz(&p,a);T(3);
        a&=0x10u;c3_nz(&p,a);T(2);
        T(2u+2u*(!(p&2u))); /* BNE $FD40 */
        if(!(p&2u)){goto low_nibble;}
        T(4); /* BEQ $FD32 */
        goto high_nibble;
    }
    /* Horizontal collision address computation, $FCE3-$FD30. */
    sp++;a=ram[0x100u+sp];c3_nz(&p,a);T(4);
    p&=(uint8_t)~1u;T(2);
    a=c3_add(a,ram[0x438],&p);T(4);
    y=a;c3_nz(&p,y);T(2);
    a=x;c3_nz(&p,a);T(2);
    p&=(uint8_t)~1u;T(2);
    a=c3_add(a,ram[0x41c],&p);T(4);
    p|=1u;T(2);
    a=c3_sub(a,0x20u,&p);T(2);
    T(2u+2u*(!(p&1u))); /* BCC $FD4A */
    if(!(p&1u)){exit_pc=0xFD4C;goto zero_exit;}
    c3_cmp(a,0xc0u,&p);T(2);
    T(2u+2u*((p&1u)!=0)); /* BCS $FD4A */
    if(p&1u){exit_pc=0xFD4C;goto zero_exit;}
    ram[0x11]=a;T(3);
    ram[0x10]=y;T(3);
    for(unsigned i=0;i<4;++i){ram[0x11]=c3_lsr(ram[0x11],&p);T(5);}
    a=ram[0x53];c3_nz(&p,a);T(3);
    p&=(uint8_t)~1u;T(2);
    a=c3_add(a,ram[0x10],&p);T(3);
    ram[0x13]=a;T(3);
    a=ram[0x54];c3_nz(&p,a);T(3);
    a=c3_add(a,0,&p);T(2);
    ram[0x14]=a;T(3);
    y=a;c3_nz(&p,y);T(2);
    a=ram[0x13];c3_nz(&p,a);T(3);
    for(unsigned i=0;i<5;++i){a=c3_lsr(a,&p);T(2);}
    a|=fixed_bank[0x3d62u+y];c3_nz(&p,a);T(4u+(y>=0x9eu));
    /* ROM wraps the table-derived page into its 12-entry lookup. */
    for(;;) {
        c3_cmp(a,12,&p);T(2);
        T(2u+(!(p&1u))); /* BCC $FD24 */
        if(!(p&1u))break;
        a=c3_sub(a,12,&p);T(2);
        T(3); /* JMP $FD1B */
        if(steps>256u)return -3;
    }
    y=a;c3_nz(&p,y);T(2);
    a=fixed_bank[0x3d4du+y];c3_nz(&p,a);T(4);
    p&=(uint8_t)~1u;T(2);
    a=c3_add(a,ram[0x11],&p);T(3);
    y=a;c3_nz(&p,y);T(2);
    a=ram[0x13];c3_nz(&p,a);T(3);
    a&=0x10u;c3_nz(&p,a);T(2);
    T(2u+((p&2u)==0)); /* BNE $FD40 */
    if(!(p&2u))goto low_nibble;
high_nibble:
    a=0;c3_nz(&p,a);T(2);
    ram[0xa2]=a;T(3);
    a=ram[0x6e0u+y];c3_nz(&p,a);T(4u+(y>=0x20u));
    for(unsigned i=0;i<4;++i){a=c3_lsr(a,&p);T(2);}
    c3_cmp(a,0,&p);T(2);
    exit_pc=0xFD3F;goto done;
low_nibble:
    a=1;c3_nz(&p,a);T(2);
    ram[0xa2]=a;T(3);
    a=ram[0x6e0u+y];c3_nz(&p,a);T(4u+(y>=0x20u));
    a&=0x0fu;c3_nz(&p,a);T(2);
    exit_pc=0xFD49;goto done;
zero_exit:
    a=0;c3_nz(&p,a);T(2);
done:
    T(6); /* RTS */
    *out=(CviiiCollisionNativeResult){a,x,y,sp,p,steps,cycles,exit_pc};
    return 0;
}

#undef T
/* ========== BOUNDED TRANSLATION: cviii_collision_callers_native_v3.h ========== */
/* Native translation of bounded post-$FCDE caller logic in Japanese PRG bank 14.
 * ROM-specific addresses below are PHYSICAL-BANK matched by the test suite.
 * A result is a boundary, never a claim that external routines were executed.
 * Does not interpret opcodes, generate movement physics, or access PPU.
 */
typedef struct {
    uint8_t a,x,y,p;
    uint8_t ram[2048];
} CviiiCollisionCpuV3;
typedef enum {
    CVIII_NEXT_COLLISION=1, CVIII_NEXT_EXTERNAL=2,
    CVIII_NEXT_RETURN=3, CVIII_NEXT_FALLTHROUGH=4,
    CVIII_NEXT_UNSUPPORTED=5, CVIII_NEXT_JUMP=6
} CviiiCallerExitV3;
typedef struct { uint16_t pc; CviiiCallerExitV3 kind; } CviiiCallerResultV3;
static void cviii_nz_v3(CviiiCollisionCpuV3 *s,uint8_t n) {
    s->p=(uint8_t)((s->p&~0x82u)|(n?0:0x02u)|(n&0x80u));
}
static void cviii_lda_v3(CviiiCollisionCpuV3*s,uint8_t a){s->a=a;cviii_nz_v3(s,a);}
static void cviii_ldx_v3(CviiiCollisionCpuV3*s,uint8_t x){s->x=x;cviii_nz_v3(s,x);}
static void cviii_ldy_v3(CviiiCollisionCpuV3*s,uint8_t y){s->y=y;cviii_nz_v3(s,y);}
static CviiiCallerResultV3 cviii_result_v3(uint16_t pc,CviiiCallerExitV3 kind){
    CviiiCallerResultV3 r={pc,kind};return r;
}
/* One fixed, authenticated PRG-bank-14 collision caller. ROM JSR itself is
 * separately indexed; this function starts after its simulated RTS.
 * For 33 call sites the first instruction is an ordinary Z-flag branch.
 * $839D instead begins two comparisons and is handled in the bounded path.
 */
typedef struct {uint16_t call,branch,target;} CviiiCallerBranchV3;
static const CviiiCallerBranchV3 cviii_first_branches_v3[]={
 {0x8345,0xd0,0x8353},{0x834e,0xf0,0x8359},
 {0x83ea,0xf0,0x83f5},{0x83f6,0xf0,0x83fd},
 {0x8429,0xf0,0x844c},{0x8450,0xf0,0x8479},
 {0x84cf,0xd0,0x84e4},{0x84d8,0xd0,0x84e4},
 {0x9153,0xf0,0x915a},{0x9164,0xd0,0x9158},
 {0x9212,0xd0,0x9228},
 {0x96cf,0xd0,0x96f8},{0x96d8,0xd0,0x96f8},
 {0x96e4,0xd0,0x96f8},{0x96ed,0xd0,0x96f8},
 {0x970b,0xd0,0x96f8},{0x9714,0xd0,0x96f8},
 {0x9720,0xd0,0x96f8},{0x9729,0xd0,0x96f8},
 {0x97b1,0xd0,0x97d6},{0x97ba,0xd0,0x97d6},
 {0x97c5,0xd0,0x97d6},{0x97ce,0xd0,0x97d6},
 {0x97e7,0xf0,0x97f8},{0x97fc,0xf0,0x980d},
 {0x9b3c,0xd0,0x9b55},{0x9b45,0xf0,0x9b9b},
 {0x9b4e,0xf0,0x9b9b},{0x9b59,0xf0,0x9b9b},
 {0x9b7b,0xd0,0x9b92},{0x9b84,0xf0,0x9b9b},
 {0x9b8d,0xf0,0x9b5e},{0x9b96,0xf0,0x9b5e}
};
static int cviii_collision_first_v3(uint16_t call,uint8_t p,uint16_t *next){
    for(size_t i=0;i<sizeof(cviii_first_branches_v3)/sizeof(cviii_first_branches_v3[0]);++i){
        const CviiiCallerBranchV3 *r=&cviii_first_branches_v3[i];
        if(r->call!=call)continue;
        const int taken=r->branch==0xf0 ? !!(p&0x02u) : !(p&0x02u);
        if(next)*next=taken?r->target:(uint16_t)(call+5u);
        return taken;
    }
    return -1;
}
/* Native meaning of a resolved $96xx collision result. No asserted RAM semantics. Boundary callbacks
 * carry the ORIGINAL register values for the next ROM query.
 */
static CviiiCallerResultV3 cviii_write_state_pair_v3(CviiiCollisionCpuV3*s,uint8_t value){
    cviii_lda_v3(s,value);
    cviii_ldy_v3(s,0);
    s->ram[0x0509]=0;
    s->ram[0x04f2]=s->a;
    return cviii_result_v3(0x968a,CVIII_NEXT_EXTERNAL);
}
static CviiiCallerResultV3 cviii_query_v3(CviiiCollisionCpuV3*s,
                                            uint8_t x,uint8_t a,uint16_t pc){
    cviii_ldx_v3(s,x);cviii_lda_v3(s,a);
    return cviii_result_v3(pc,CVIII_NEXT_COLLISION);
}
/* V9: remaining 16 ROM-return continuations, bounded at unresolved calls.
 * The initial flags are those returned by $FCDE; these functions do not
 * execute the JSR $FCDE again. RAM offsets and comparison order are literal.
 */
static void cviii_cmp_v9(CviiiCollisionCpuV3*s,uint8_t b){
    c3_cmp(s->a,b,&s->p);
}
static void cviii_adc_v9(CviiiCollisionCpuV3*s,uint8_t b){
    s->a=c3_add(s->a,b,&s->p);
}
static CviiiCallerResultV3 cviii_collision_remaining_v9(
    uint16_t call,CviiiCollisionCpuV3*s){
    int z=(s->p&2u)!=0;
    switch(call){
    case 0x8345:
        if(!z)return cviii_result_v3(0x8353,CVIII_NEXT_EXTERNAL);
        cviii_lda_v3(s,s->ram[0x08]);cviii_ldx_v3(s,0);
        return cviii_result_v3(0x834e,CVIII_NEXT_COLLISION);
    case 0x834e:
        return cviii_result_v3(z?0x8359:0x8353,CVIII_NEXT_EXTERNAL);
    case 0x83ea:
        if(!z){
            cviii_cmp_v9(s,1);
            if(!(s->p&2u))cviii_cmp_v9(s,8);
        }
        return cviii_result_v3(0x83f5,CVIII_NEXT_RETURN);
    case 0x83f6:
        if(!z)cviii_cmp_v9(s,1);
        return cviii_result_v3(0x83fd,CVIII_NEXT_RETURN);
    case 0x8429:
    case 0x8450: {
        if(z){
            if(call==0x8450){s->p&=(uint8_t)~1u;
                return cviii_result_v3(0x847a,CVIII_NEXT_RETURN);}
            cviii_ldx_v3(s,s->ram[0x08]);cviii_lda_v3(s,0xfbu);
            return cviii_result_v3(0x8450,CVIII_NEXT_COLLISION);
        }
        const uint8_t tests[]={1,2,3,4,5,7};
        const uint16_t path_8429[]={0x84ab,0x847b,0x8495,0x83ca,0x83fe,0x83fe};
        const uint16_t path_8450[]={0x84b4,0x847b,0x8495,0x83da,0x83fe,0x83fe};
        for(unsigned i=0;i<6;++i){
            cviii_cmp_v9(s,tests[i]);
            if(s->p&2u)return cviii_result_v3(
                call==0x8429?path_8429[i]:path_8450[i],CVIII_NEXT_FALLTHROUGH);
        }
        cviii_cmp_v9(s,0x0cu);
        if(s->p&1u)return cviii_result_v3(
                call==0x8429?0x83ca:0x83da,CVIII_NEXT_FALLTHROUGH);
        s->p|=1u;
        return cviii_result_v3(0x8478,CVIII_NEXT_RETURN);
    }
    case 0x84cf:
        if(z){cviii_ldx_v3(s,s->ram[0x08]);cviii_lda_v3(s,0xfbu);
            return cviii_result_v3(0x84d8,CVIII_NEXT_COLLISION);}
        goto shared_84e4;
    case 0x84d8:
        if(z){cviii_ldx_v3(s,s->ram[0x8e]);
            return cviii_result_v3(0x84df,CVIII_NEXT_EXTERNAL);}
shared_84e4:
        cviii_lda_v3(s,0);s->ram[0x88]=0;
        return cviii_result_v3(0x84e8,CVIII_NEXT_EXTERNAL);
    case 0x9153:
        if(!z){s->p|=1u;return cviii_result_v3(0x9159,CVIII_NEXT_RETURN);}
        cviii_ldy_v3(s,s->ram[0x7f]);s->ram[0x01]=s->y;
        return cviii_result_v3(0x915e,CVIII_NEXT_FALLTHROUGH);
    case 0x9164:
        if(!z){s->p|=1u;return cviii_result_v3(0x9159,CVIII_NEXT_RETURN);}
        cviii_lda_v3(s,s->ram[0x01]);s->p&=(uint8_t)~1u;
        cviii_adc_v9(s,4);cviii_cmp_v9(s,8);
        if(s->p&1u){s->p&=(uint8_t)~1u;
            return cviii_result_v3(0x9176,CVIII_NEXT_RETURN);}
        cviii_ldy_v3(s,s->a);
        if(!s->y){s->p&=(uint8_t)~1u;
            return cviii_result_v3(0x9176,CVIII_NEXT_RETURN);}
        s->ram[0x01]=s->y;
        return cviii_result_v3(0x915e,CVIII_NEXT_FALLTHROUGH);
    case 0x97b1:
        if(z){cviii_ldx_v3(s,8);cviii_lda_v3(s,0xf8u);
            return cviii_result_v3(0x97ba,CVIII_NEXT_COLLISION);}
        goto shared_97d6;
    case 0x97ba:
    case 0x97ce:
        if(z)return cviii_result_v3(0x97d3,CVIII_NEXT_EXTERNAL);
        goto shared_97d6;
    case 0x97c5:
        if(z){cviii_ldx_v3(s,8);cviii_lda_v3(s,8);
            return cviii_result_v3(0x97ce,CVIII_NEXT_COLLISION);}
shared_97d6:
        cviii_lda_v3(s,s->ram[0x041c]);s->ram[0x06]=s->a;
        return cviii_result_v3(0x97db,CVIII_NEXT_EXTERNAL);
    case 0x97e7:
        if(z){cviii_ldx_v3(s,0x10);cviii_lda_v3(s,5);
            return cviii_result_v3(0x97fc,CVIII_NEXT_COLLISION);}
        cviii_ldx_v3(s,8);cviii_lda_v3(s,0xfbu);
        return cviii_result_v3(0x97f0,CVIII_NEXT_EXTERNAL);
    case 0x97fc:
        if(!z){cviii_ldx_v3(s,8);cviii_lda_v3(s,5);
            return cviii_result_v3(0x9805,CVIII_NEXT_EXTERNAL);}
        cviii_lda_v3(s,s->ram[0x88]);cviii_cmp_v9(s,1);
        if(s->p&2u){cviii_ldx_v3(s,s->ram[0x8e]);
            return cviii_result_v3(0x981f,CVIII_NEXT_EXTERNAL);}
        cviii_cmp_v9(s,5);
        if(!(s->p&2u)){cviii_cmp_v9(s,6);
            if(!(s->p&2u))return cviii_result_v3(0x9848,CVIII_NEXT_JUMP);}
        cviii_lda_v3(s,s->ram[0x06]);s->p&=(uint8_t)~1u;
        cviii_adc_v9(s,s->ram[0x8e]);s->ram[0x041c]=s->a;
        cviii_lda_v3(s,2);s->ram[0x0565]=s->a;
        return cviii_result_v3(0x9847,CVIII_NEXT_RETURN);
    default:return cviii_result_v3(0,CVIII_NEXT_UNSUPPORTED);
    }
}
/* Starting point: the first instruction AFTER the JSR at call.
 * Return boundary is another collision JSR, an external JSR/JMP, or RTS.
 */
static CviiiCallerResultV3 cviii_collision_caller_native_v3(
                                 uint16_t call,CviiiCollisionCpuV3*s){
    if(!s)return cviii_result_v3(0,CVIII_NEXT_UNSUPPORTED);
    const int zero=(s->p&0x02u)!=0;
    switch(call){
    case 0x96cf: return zero?cviii_query_v3(s,0xf0,0x08,0x96d8):
                          cviii_write_state_pair_v3(s,0);
    case 0x96d8: return zero?cviii_result_v3(0x968d,CVIII_NEXT_EXTERNAL):
                          cviii_write_state_pair_v3(s,0);
    case 0x96e4: return zero?cviii_query_v3(s,0xf0,0x08,0x96ed):
                          cviii_write_state_pair_v3(s,0);
    case 0x96ed: return cviii_write_state_pair_v3(s,zero?0x01:0);
    case 0x970b: return zero?cviii_query_v3(s,0xf0,0xf8,0x9714):
                          cviii_write_state_pair_v3(s,0);
    case 0x9714: return zero?cviii_result_v3(0x968d,CVIII_NEXT_EXTERNAL):
                          cviii_write_state_pair_v3(s,0);
    case 0x9720: return zero?cviii_query_v3(s,0xf0,0xf8,0x9729):
                          cviii_write_state_pair_v3(s,0);
    case 0x9729: return cviii_write_state_pair_v3(s,zero?0xff:0);
    case 0x9b3c: return zero?cviii_query_v3(s,0x12,0xfe,0x9b45):
                          cviii_query_v3(s,0x1a,0x02,0x9b59);
    case 0x9b45: if(!zero)return cviii_query_v3(s,0x1a,0xfe,0x9b4e);break;
    case 0x9b4e: if(!zero)return cviii_result_v3(0x9b5e,CVIII_NEXT_EXTERNAL);break;
    case 0x9b59: if(!zero)return cviii_result_v3(0x9b5e,CVIII_NEXT_EXTERNAL);break;
    case 0x9b7b: return zero?cviii_query_v3(s,0x12,0xfe,0x9b84):
                          cviii_query_v3(s,0x0a,0x02,0x9b96);
    case 0x9b84: if(!zero)return cviii_query_v3(s,0x0a,0xfe,0x9b8d);break;
    case 0x9b8d: if(zero)return cviii_result_v3(0x9b5e,CVIII_NEXT_EXTERNAL);
                 return cviii_query_v3(s,0x0a,0x02,0x9b96);
    case 0x9b96: if(zero)return cviii_result_v3(0x9b5e,CVIII_NEXT_EXTERNAL);break;
    case 0x9212:
        if(!zero)return cviii_result_v3(0x9228,CVIII_NEXT_FALLTHROUGH);
        cviii_ldx_v3(s,s->ram[0x008e]);
        cviii_lda_v3(s,s->ram[0x0509+s->x]);s->ram[0x0509]=s->a;
        cviii_lda_v3(s,s->ram[0x04f2+s->x]);s->ram[0x04f2]=s->a;
        return cviii_result_v3(0x8723,CVIII_NEXT_EXTERNAL);
    case 0x839d: {
        uint16_t destination=0x83aa;
        for(unsigned i=0;i<2;++i){
            const uint8_t v=i?7:5;
            int difference=(int)s->a-(int)v;
            s->p=(uint8_t)((s->p&~0x83u)|
                  (difference>=0?1u:0u)|((uint8_t)difference==0?2u:0u)|
                  ((uint8_t)difference&0x80u));
            if(difference==0)return cviii_result_v3(destination,CVIII_NEXT_FALLTHROUGH);
        }
        s->p=(uint8_t)(s->p&~1u); /* CLC before RTS */
        return cviii_result_v3(0x83a9,CVIII_NEXT_RETURN);
    }
    default:return cviii_collision_remaining_v9(call,s);
    }
    /* Shared $9B9B successful collision exit. LDA #$12; STA $0565; RTS. */
    cviii_lda_v3(s,0x12);s->ram[0x0565]=0x12;
    return cviii_result_v3(0x9ba0,CVIII_NEXT_RETURN);
}

/* ========== BOUNDED TRANSLATION: cviii_collision_dependencies_native_v4.h ========== */
/* Japanese Mapper 24, physical PRG bank 14. Native, bounded translation.
   The 2A03's decimal flag does NOT enable decimal arithmetic.
   Callers must simulate JSR/RTS stack effects and CPU cycles elsewhere.
   External $E648 is an explicit suspension, not an invented implementation. */
typedef struct {
    uint8_t a, x, y, p;
    uint8_t ram[2048];
} CviiiDepCpuV4;
typedef struct { uint16_t pc; uint8_t kind; } CviiiDepExitV4;
enum { CVIII_DEP_RTS=1, CVIII_DEP_JSR=2, CVIII_DEP_JMP=3,
       CVIII_DEP_BAD_ENTRY=4 };
static CviiiDepExitV4 cviii_dep_exit_v4(unsigned pc,unsigned kind) {
    CviiiDepExitV4 r={(uint16_t)pc,(uint8_t)kind};return r;
}
static void cviii_dep_nz_v4(CviiiDepCpuV4 *s,uint8_t v) {
    s->p=(uint8_t)((s->p&~0x82u)|(v?0:0x02u)|(v&0x80u));
}
static void cviii_dep_a_v4(CviiiDepCpuV4*s,uint8_t v) {s->a=v;cviii_dep_nz_v4(s,v);}
static void cviii_dep_x_v4(CviiiDepCpuV4*s,uint8_t v) {s->x=v;cviii_dep_nz_v4(s,v);}
static void cviii_dep_y_v4(CviiiDepCpuV4*s,uint8_t v) {s->y=v;cviii_dep_nz_v4(s,v);}
static void cviii_dep_cmp_v4(CviiiDepCpuV4*s,uint8_t left,uint8_t right) {
    uint8_t diff=(uint8_t)(left-right);
    s->p=(uint8_t)((s->p&~0x83u)|(left>=right?1u:0u)|
                   (diff?0:2u)|(diff&0x80u));
}
static void cviii_dep_adc_v4(CviiiDepCpuV4*s,uint8_t rhs) {
    const uint8_t before=s->a;
    unsigned sum=(unsigned)before+rhs+(s->p&1u);
    uint8_t result=(uint8_t)sum;
    s->p=(uint8_t)((s->p&~0x41u)|(sum>255?1u:0u)|
        ((~(before^rhs)&(before^result)&0x80u)?0x40u:0u));
    cviii_dep_a_v4(s,result);
}
static void cviii_dep_sbc_v4(CviiiDepCpuV4*s,uint8_t rhs) {
    const uint8_t before=s->a;
    int diff=(int)before-(int)rhs-((s->p&1u)?0:1);
    uint8_t result=(uint8_t)diff;
    s->p=(uint8_t)((s->p&~0x41u)|(diff>=0?1u:0u)|
        (((before^result)&(before^rhs)&0x80u)?0x40u:0u));
    cviii_dep_a_v4(s,result);
}
/* Complete two-path ROM subroutine $89EB-$8A09. */
static CviiiDepExitV4 cviii_collision_89eb_v4(CviiiDepCpuV4*s) {
    if(!s)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    cviii_dep_a_v4(s,s->ram[0x65]);
    if(!(s->p&0x80u)) {
        cviii_dep_a_v4(s,s->ram[0x041c]);
        cviii_dep_a_v4(s,(uint8_t)(s->a&0xf0u));
        s->ram[0x041c]=s->a;
        return cviii_dep_exit_v4(0x89f7,CVIII_DEP_RTS);
    }
    s->p=(uint8_t)(s->p&~1u); /* CLC */
    cviii_dep_a_v4(s,s->ram[0x041c]);
    cviii_dep_adc_v4(s,s->ram[0x53]);
    cviii_dep_a_v4(s,(uint8_t)(s->a&0xf0u));
    s->p=(uint8_t)(s->p|1u); /* SEC */
    cviii_dep_sbc_v4(s,s->ram[0x53]);
    s->p=(uint8_t)(s->p&~1u);
    cviii_dep_adc_v4(s,3);
    s->ram[0x041c]=s->a;
    return cviii_dep_exit_v4(0x8a09,CVIII_DEP_RTS);
}
/* Composite translated $9B5E: JSR $89EB, then conditional write to
   $041C and unconditional $0565. Stack/cycle effects of JSR are not modeled. */
static CviiiDepExitV4 cviii_collision_9b5e_v4(CviiiDepCpuV4*s) {
    if(!s)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    (void)cviii_collision_89eb_v4(s);
    cviii_dep_a_v4(s,s->ram[0x054e]);
    cviii_dep_cmp_v4(s,s->a,2);
    if(s->p&2u) {
        cviii_dep_a_v4(s,s->ram[0x041c]);
        s->p=(uint8_t)(s->p&~1u);
        cviii_dep_adc_v4(s,4);
        s->ram[0x041c]=s->a;
    }
    cviii_dep_a_v4(s,2);
    s->ram[0x0565]=s->a;
    return cviii_dep_exit_v4(0x9b76,CVIII_DEP_RTS);
}
static CviiiDepExitV4 cviii_collision_8723_resume_v4(CviiiDepCpuV4*s,uint16_t pc);
/* ROM $8723 entry. Proceeds to a real RTS, or suspends at the JSR $E648.
   When external JSR is encountered, caller must execute the genuine routine
   (including its CPU stack effects) before calling resume_8723_v4(). */
static CviiiDepExitV4 cviii_collision_8723_v4(CviiiDepCpuV4*s) {
    if(!s)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    cviii_dep_a_v4(s,s->ram[0x88]);
    cviii_dep_cmp_v4(s,s->a,2);
    if(s->p&2u) {
        cviii_dep_y_v4(s,s->ram[0x04f2]);
        if(s->p&0x80u) {
            cviii_dep_a_v4(s,s->ram[0x8d]);
            if(!(s->p&2u))return cviii_dep_exit_v4(0x871c,CVIII_DEP_RTS);
        } else {
            cviii_dep_a_v4(s,s->ram[0x8d]);
            if(s->p&2u)return cviii_dep_exit_v4(0x871c,CVIII_DEP_RTS);
        }
    }
    cviii_dep_a_v4(s,s->ram[0x75]);
    if(s->p&2u) {
        cviii_dep_a_v4(s,s->ram[0x65]);
        if(!(s->p&0x80u)) {
            cviii_dep_a_v4(s,s->ram[0x04f2]);
            if(s->p&0x80u) {
                cviii_dep_a_v4(s,s->ram[0xc5]);
                if(s->p&2u) {
                    cviii_dep_a_v4(s,s->ram[0x53]);
                    cviii_dep_a_v4(s,(uint8_t)(s->a|s->ram[0x54]));
                    if(!(s->p&2u)) {
                        cviii_dep_y_v4(s,s->ram[0x0438]);
                        cviii_dep_y_v4(s,(uint8_t)(s->y-1u));
                        if(!(s->p&0x80u)) {
                            cviii_dep_a_v4(s,0);s->ram[0x62]=s->a;
                            return cviii_dep_exit_v4(0x877f,CVIII_DEP_RTS);
                        }
                    }
                }
            } else {
                cviii_dep_a_v4(s,(uint8_t)(s->a|s->ram[0x0509]));
                if(s->p&2u)return cviii_dep_exit_v4(0x876a,CVIII_DEP_RTS);
                cviii_dep_a_v4(s,s->ram[0x6e]);
                cviii_dep_cmp_v4(s,s->a,s->ram[0x54]);
                if(!(s->p&2u)) {
                    cviii_dep_a_v4(s,s->ram[0x0438]);
                    if(s->p&0x80u) {
                        cviii_dep_a_v4(s,1);s->ram[0x62]=s->a;
                        return cviii_dep_exit_v4(0x874a,CVIII_DEP_RTS);
                    }
                }
            }
        }
    }
    cviii_dep_a_v4(s,s->ram[0x04c4]);
    s->p=(uint8_t)(s->p&~1u);
    cviii_dep_adc_v4(s,s->ram[0x0509]);s->ram[0x04c4]=s->a;
    cviii_dep_a_v4(s,s->ram[0x0438]);
    cviii_dep_adc_v4(s,s->ram[0x04f2]);s->ram[0x0438]=s->a;
    cviii_dep_y_v4(s,s->ram[0x65]);
    if(!(s->p&0x80u)) {
        cviii_dep_cmp_v4(s,s->a,0x18);
        if(s->p&1u) {
            cviii_dep_cmp_v4(s,s->a,0xe9);
            if(!(s->p&1u))return cviii_dep_exit_v4(0x876a,CVIII_DEP_RTS);
            cviii_dep_x_v4(s,1);
        } else cviii_dep_x_v4(s,0);
        return cviii_dep_exit_v4(0x8786,CVIII_DEP_JSR); /* $E648 */
    }
    return cviii_collision_8723_resume_v4(s,0x878b); /* internal continuation */
}
/* Resumes after the genuine external JSR $E648, or enters at $878B when
   the Y-negative path bypasses the JSR. Unchanged 6502 A/X/Y/P supplied. */
static CviiiDepExitV4 cviii_collision_8723_resume_v4(CviiiDepCpuV4*s,uint16_t pc) {
    if(!s)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    if(pc==0x8789 && (s->p&1u)) {
        cviii_dep_cmp_v4(s,s->x,0); /* CPX #0 */
        cviii_dep_a_v4(s,(s->p&2u)?0x17:0xe9);
        s->ram[0x0438]=s->a;
        return cviii_dep_exit_v4(0x87c8,CVIII_DEP_RTS);
    }
    if(pc!=0x8789 && pc!=0x878b)
        return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    cviii_dep_a_v4(s,s->ram[0x0438]);
    cviii_dep_cmp_v4(s,s->a,0x10);
    if(!(s->p&1u)) {
        cviii_dep_x_v4(s,0);cviii_dep_a_v4(s,0x10);
    } else {
        cviii_dep_cmp_v4(s,s->a,0xf0);
        if(!(s->p&1u))return cviii_dep_exit_v4(0x87ba,CVIII_DEP_RTS);
        cviii_dep_x_v4(s,1);cviii_dep_a_v4(s,0xf0);
    }
    s->ram[0x0438]=s->a;
    cviii_dep_a_v4(s,s->ram[0x054e]);
    cviii_dep_cmp_v4(s,s->a,3);
    if(!(s->p&2u))return cviii_dep_exit_v4(0x87ba,CVIII_DEP_RTS);
    cviii_dep_a_v4(s,s->ram[0x0565]);
    cviii_dep_cmp_v4(s,s->a,0x34);
    if(!(s->p&2u))return cviii_dep_exit_v4(0x87ba,CVIII_DEP_RTS);
    cviii_dep_a_v4(s,0x36);s->ram[0x0565]=s->a;
    s->ram[0x05d8]=s->x;
    return cviii_dep_exit_v4(0x87b9,CVIII_DEP_RTS);
}

/* ========== BOUNDED TRANSLATION: cviii_collision_e648_native_v5.h ========== */
/* ROM-specific bounded high-level translation of $E648 -> bank7:$B437.
   Source: Akumajou Densetsu (Japan), SHA256 958a4b...7a28.
   Requires the authenticated 393232-byte ROM, not executable ROM parsing.
   Carries synthetic CPU registers, RAM, mapper register and 6502 stack.
   Not a cycle-accurate emulator; no IRQ/NMI can occur inside this call.
   Non-mapped pointer reads fail closed (no synthetic value supplied). */

typedef struct {
    CviiiDepCpuV4 cpu;
    uint8_t sp;                /* 6502 stack pointer before JSR $E648 */
    uint8_t mapped_prg_bank;   /* bank at CPU $8000-$BFFF */
    unsigned mapper_writes;    /* observable writes to $8000 */
} CviiiCollisionMachineV5;
typedef struct {
    uint16_t next_pc;
    uint8_t ok;                /* 0: unsupported ROM/bus/mapper state */
    uint8_t collision_carry;   /* C returned by bank7:$B437 */
} CviiiCollisionExitV5;

static void cv5_nz(CviiiCollisionMachineV5 *m, uint8_t v) {
    m->cpu.p=(uint8_t)((m->cpu.p&~0x82u)|(v?0:2u)|(v&0x80u));
}
static void cv5_a(CviiiCollisionMachineV5*m,uint8_t v) {m->cpu.a=v;cv5_nz(m,v);}
static void cv5_y(CviiiCollisionMachineV5*m,uint8_t v) {m->cpu.y=v;cv5_nz(m,v);}
static void cv5_cmp(CviiiCollisionMachineV5*m,uint8_t left,uint8_t right) {
    uint8_t dif=(uint8_t)(left-right);
    m->cpu.p=(uint8_t)((m->cpu.p&~0x83u)|(left>=right?1u:0u)|(!dif?2u:0u)|(dif&0x80u));
}
static void cv5_sbc(CviiiCollisionMachineV5*m,uint8_t value) {
    uint8_t b=m->cpu.a;
    int sum=(int)b-value-((m->cpu.p&1u)?0:1);
    uint8_t v=(uint8_t)sum;
    m->cpu.p=(uint8_t)((m->cpu.p&~0x41u)|(sum>=0?1u:0u)|
        (((b^v)&(b^value)&0x80u)?0x40u:0u));
    cv5_a(m,v);
}
static void cv5_asl_a(CviiiCollisionMachineV5*m) {
    uint8_t old=m->cpu.a;
    m->cpu.p=(uint8_t)((m->cpu.p&~1u)|((old>>7)&1u));
    cv5_a(m,(uint8_t)(old<<1));
}
static void cv5_push(CviiiCollisionMachineV5*m,uint8_t value) {
    m->cpu.ram[0x100u+m->sp]=value;
    m->sp=(uint8_t)(m->sp-1u);
}
static uint8_t cv5_pop(CviiiCollisionMachineV5*m) {
    m->sp=(uint8_t)(m->sp+1u);
    return m->cpu.ram[0x100u+m->sp];
}
static void cv5_jsr(CviiiCollisionMachineV5*m,uint16_t return_pc_minus_1) {
    cv5_push(m,(uint8_t)(return_pc_minus_1>>8));
    cv5_push(m,(uint8_t)return_pc_minus_1);
}
static void cv5_rts(CviiiCollisionMachineV5*m) {(void)cv5_pop(m);(void)cv5_pop(m);}
/* ROM data accesses in bank 7; CPU RAM has NES $0000-$07FF mirror. */
static int cv5_read(const CviiiCollisionMachineV5*m,const uint8_t*rom,
                    size_t rom_len,uint16_t address,uint8_t *out) {
    if(address<0x2000) {*out=m->cpu.ram[address&0x07ffu];return 1;}
    /* CPU $E000-$FFFF: fixed last 8 KiB of the PRG ROM, physical file $3E010-$4000F. */
    if(address>=0xe000) {
        size_t off=0x3e010u+(size_t)(address-0xe000u);
        if(off>=rom_len)return 0;
        *out=rom[off];return 1;
    }
    if(address>=0x8000 && address<0xc000 && m->mapped_prg_bank<16) {
        size_t off=16u+(size_t)m->mapped_prg_bank*0x4000u+(address-0x8000u);
        if(off>=rom_len)return 0;
        *out=rom[off];return 1;
    }
    return 0; /* I/O and unrepresented cartridge mappings must not be guessed */
}
static int cv5_rom_y(CviiiCollisionMachineV5*m,const uint8_t*rom,size_t len,
                     uint16_t address,uint8_t*out) {
    return cv5_read(m,rom,len,(uint16_t)(address+m->cpu.y),out);
}
static int cv5_ind_y(CviiiCollisionMachineV5*m,const uint8_t*rom,size_t len,
                     uint8_t*out) {
    uint16_t ptr=(uint16_t)(m->cpu.ram[8]|((unsigned)m->cpu.ram[9]<<8));
    return cv5_rom_y(m,rom,len,ptr,out);
}
static void cv5_inc_y(CviiiCollisionMachineV5*m) {
    cv5_y(m,(uint8_t)(m->cpu.y+1u));
}
/* Bank 7 $B437-$B4C7, translated from authentic ROM; no opcode interpreter. */
static int cv5_bank7_b437(CviiiCollisionMachineV5*m,const uint8_t*rom,size_t len) {
    uint8_t v;
    cv5_a(m,m->cpu.ram[0x53]);
    if(m->cpu.a) goto miss;
    cv5_a(m,m->cpu.ram[0x34]);cv5_asl_a(m);cv5_y(m,m->cpu.a);
    if(!cv5_rom_y(m,rom,len,0xb5f6,&v))return 0;
    cv5_a(m,v);m->cpu.ram[8]=v;
    if(!cv5_rom_y(m,rom,len,0xb5f7,&v))return 0;
    cv5_a(m,v);m->cpu.ram[9]=v;
    cv5_a(m,m->cpu.ram[0x35]);cv5_asl_a(m);
    m->cpu.ram[0]=m->cpu.a;cv5_asl_a(m);
    m->cpu.p&=(uint8_t)~1u; /* CLC */
    cviii_dep_adc_v4(&m->cpu,m->cpu.ram[0]);
    cv5_y(m,m->cpu.a);
    if(!cv5_ind_y(m,rom,len,&v))return 0;
    cv5_a(m,v);cv5_cmp(m,m->cpu.a,m->cpu.ram[0x36]);
    if(!(m->cpu.p&2u))goto miss;
    cv5_inc_y(m);
    if(!cv5_ind_y(m,rom,len,&v))return 0;
    cv5_a(m,v);cv5_cmp(m,m->cpu.a,m->cpu.ram[0x54]);
    if(!(m->cpu.p&2u))goto miss;
    cv5_inc_y(m);cv5_a(m,m->cpu.x); /* TXA */
    if(!cv5_ind_y(m,rom,len,&v))return 0;
    cv5_cmp(m,m->cpu.a,v);
    if(!(m->cpu.p&2u))goto miss;
    cv5_inc_y(m);
    if(!cv5_ind_y(m,rom,len,&v))return 0;
    cv5_a(m,v);cv5_inc_y(m);
    m->cpu.p|=1u;cv5_sbc(m,m->cpu.ram[0x041c]);
    if(!(m->cpu.p&1u))cv5_a(m,(uint8_t)(m->cpu.a^0xffu));
    cv5_cmp(m,m->cpu.a,8);
    if(m->cpu.p&1u) {
        if(!cv5_ind_y(m,rom,len,&v))return 0;
        cv5_a(m,v);m->cpu.p|=1u;
        cv5_sbc(m,m->cpu.ram[0x041c]);
        if(!(m->cpu.p&1u))cv5_a(m,(uint8_t)(m->cpu.a^0xffu));
        cv5_cmp(m,m->cpu.a,8);
        if(m->cpu.p&1u)goto hit;
    }
    cv5_a(m,m->cpu.ram[0x0565]);cv5_cmp(m,m->cpu.a,0x34);
    if(m->cpu.p&2u) {
        cv5_a(m,m->cpu.ram[0x054e]);cv5_cmp(m,m->cpu.a,3);
        if(m->cpu.p&2u) {
            cv5_a(m,0x36);m->cpu.ram[0x0565]=m->cpu.a;
            m->cpu.ram[0x05d8]=m->cpu.x;
        }
        goto hit;
    }
    cv5_cmp(m,m->cpu.a,4);
    if(!(m->cpu.p&2u))goto hit;
    cv5_inc_y(m);
    if(!cv5_ind_y(m,rom,len,&v))return 0;
    cv5_a(m,v);m->cpu.ram[0x36]=v;
    m->cpu.ram[0x62]=m->cpu.x;
    cv5_a(m,(uint8_t)(m->cpu.x^1u));m->cpu.ram[0x04a8]=m->cpu.a;
    cv5_a(m,m->cpu.ram[0x0565]);cv5_a(m,(uint8_t)(m->cpu.a|0x80u));
    m->cpu.ram[0x0565]=m->cpu.a;
    cv5_a(m,2);m->cpu.ram[0x0400]=m->cpu.a;
    cv5_a(m,8);m->cpu.ram[0x2c]=m->cpu.a;
    cv5_a(m,0);m->cpu.ram[0x68]=m->cpu.a;
 hit:m->cpu.p|=1u;return 1;
 miss:m->cpu.p&=(uint8_t)~1u;return 1;
}
/* $E648 -> $E2CA -> bank7 $B437 -> $E62D -> $E2D0 -> RTS.
   The helper is transactional: unsupported ROM bus reads leave original state untouched.
   $E2CA uses ZP $23/$24 and writes mapper register $8000. */
static CviiiCollisionExitV5 cviii_collision_e648_v5(CviiiCollisionMachineV5*m,
                                                     const uint8_t*rom,size_t len) {
    CviiiCollisionExitV5 fail={0x8786,0,0};
    if(!m||!rom||len!=393232u||memcmp(rom,"NES\x1a",4)||m->mapped_prg_bank!=m->cpu.ram[0x23])return fail;
    const size_t fixed=0x3e010u+0x648u; /* $E648 physical ROM position */
    const size_t bank7=16u+7u*0x4000u+0x3437u;
    if(memcmp(rom+fixed,"\xa9\x07\x20\xca\xe2\x20\x37\xb4\x4c\x2d\xe6",11) ||
       memcmp(rom+bank7,"\xa5\x53\xd0\x7a",4))return fail;
    CviiiCollisionMachineV5 local=*m;
    cv5_jsr(&local,0x8788);  /* original $8723 caller's JSR $E648 */
    cv5_a(&local,7);
    cv5_jsr(&local,0xe64c);  /* $E64A JSR $E2CA */
    cv5_push(&local,local.cpu.a);  /* $E2CA PHA */
    cv5_a(&local,local.cpu.ram[0x23]);
    local.cpu.ram[0x24]=local.cpu.a;
    cv5_a(&local,cv5_pop(&local));
    local.cpu.ram[0x23]=local.cpu.a; /* $E2D0 STA $23 */
    local.mapped_prg_bank=local.cpu.a;local.mapper_writes++; /* $E2D2 STA $8000 */
    cv5_rts(&local);
    cv5_jsr(&local,0xe64f); /* $E64D JSR $B437 */
    if(!cv5_bank7_b437(&local,rom,len))return fail;
    uint8_t carry=(uint8_t)(local.cpu.p&1u);
    cv5_rts(&local);
    /* $E650 JMP $E62D, $E62D LDA $24, JMP $E2D0. */
    cv5_a(&local,local.cpu.ram[0x24]);
    local.cpu.ram[0x23]=local.cpu.a;
    local.mapped_prg_bank=local.cpu.a;local.mapper_writes++;
    cv5_rts(&local);
    *m=local;
    CviiiCollisionExitV5 ok={0x8789,1,carry};return ok;
}
/* Connect v4 $8723 call suspension to the real banked native dependency.
   No longer treat $E648 as an arbitrary opaque state transition. */
static CviiiDepExitV4 cviii_collision_8723_integrated_v5(
    CviiiCollisionMachineV5*m,const uint8_t*rom,size_t len) {
    if(!m)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
    CviiiCollisionMachineV5 local=*m;
    CviiiDepExitV4 ex=cviii_collision_8723_v4(&local.cpu);
    if(ex.kind==CVIII_DEP_JSR && ex.pc==0x8786) {
        CviiiCollisionExitV5 nested=cviii_collision_e648_v5(&local,rom,len);
        if(!nested.ok)return cviii_dep_exit_v4(0,CVIII_DEP_BAD_ENTRY);
        ex=cviii_collision_8723_resume_v4(&local.cpu,nested.next_pc);
    }
    *m=local;
    return ex;
}

/* ========== BOUNDED TRANSLATION: cviii_collision_external_native_v6.h ========== */
/* Authenticated JP ROM: PRG bank 14 $8ABD and $86FB; fixed bank $EF91.
 * Readable state transformations, NOT a 6502 instruction decoder.
 * Requires v5 mapped PRG and RAM data for table-driven fixed-bank reads.
 * Unknown bus addresses cause a fail-closed transaction (caller unchanged).
 */

typedef struct { uint16_t next_pc; uint8_t ok; } CviiiCollisionExitV6;
static CviiiCollisionExitV6 cv6_result(uint16_t pc,unsigned ok) {
    CviiiCollisionExitV6 r={pc,(uint8_t)ok};return r;
}
static int cv6_rom_check(const uint8_t *rom,size_t n) {
    /* These exact entry bytes must match the authenticated ROM mapping. */
    if(!rom||n!=393232u||memcmp(rom,"NES\x1a",4))return 0;
    if(memcmp(rom+16u+14u*0x4000u+0x0abdu,"\xa9\x00\x8d\x20\x05\x8d\x37\x05\x60",9))return 0;
    if(memcmp(rom+0x3e010u+0xf91u,"\xa2\x00\xde\x7c\x05\xf0\x01\x60",8))return 0;
    if(memcmp(rom+16u+14u*0x4000u+0x06fbu,"\xa5\x65\x30\xbc",4))return 0;
    return 1;
}
static void cv6_store(CviiiCollisionMachineV5 *m,unsigned address,uint8_t v) {
    m->cpu.ram[address&0x7ffu]=v;
}
static uint8_t cv6_load(const CviiiCollisionMachineV5 *m,unsigned address) {
    return m->cpu.ram[address&0x7ffu];
}
static void cv6_cmp(CviiiCollisionMachineV5*m,uint8_t lhs,uint8_t rhs) {
    cviii_dep_cmp_v4(&m->cpu,lhs,rhs);
}
static void cv6_adc(CviiiCollisionMachineV5*m,uint8_t rhs) {
    cviii_dep_adc_v4(&m->cpu,rhs);
}
static CviiiCollisionExitV6 cviii_collision_8abd_native_v6(CviiiCollisionMachineV5*m) {
    if(!m)return cv6_result(0,0);
    cv5_a(m,0);
    cv6_store(m,0x0520u,m->cpu.a);
    cv6_store(m,0x0537u,m->cpu.a);
    return cv6_result(0x8ac5u,1);
}
/* CPU read, matching actual NES fixed-bank and mapped PRG windows; no I/O guesses. */
static int cv6_bus_read(const CviiiCollisionMachineV5*m,const uint8_t*rom,
                        size_t n,uint16_t address,uint8_t*out) {
    if(address<0x2000u) {*out=cv6_load(m,address);return 1;}
    if(address>=0xe000u) {
        size_t offset=0x3e010u+(size_t)(address-0xe000u);
        if(offset>=n)return 0;
        *out=rom[offset];return 1;
    }
    if(address>=0x8000u && address<0xc000u && m->mapped_prg_bank<16u) {
        size_t offset=16u+(size_t)m->mapped_prg_bank*0x4000u+(address-0x8000u);
        if(offset>=n)return 0;
        *out=rom[offset];return 1;
    }
    return 0;
}
/* ROM $EF91-$EFD1; X is set to zero by the original routine. Its original
 * pointer comes from fixed-bank $EFD2 indexed by RAM $048C, and 3-byte
 * records are read using 3 * RAM[$05AA] and RAM[$0593].
 * This is a transactional bounded native translation: no unsupported reads.
 */
static CviiiCollisionExitV6 cviii_collision_ef91_native_v6(
    CviiiCollisionMachineV5*m,const uint8_t*rom,size_t n) {
    if(!m||!cv6_rom_check(rom,n))return cv6_result(0xef91u,0);
    CviiiCollisionMachineV5 s=*m;
    cviii_dep_x_v4(&s.cpu,0); /* LDX #$00 */
    uint8_t countdown=(uint8_t)(cv6_load(&s,0x057cu)-1u);
    cv6_store(&s,0x057cu,countdown);cv5_nz(&s,countdown);
    if(countdown) {*m=s;return cv6_result(0xef98u,1);}
    cv5_y(&s,cv6_load(&s,0x048cu));
    uint8_t lo,hi;
    if(!cv6_bus_read(&s,rom,n,(uint16_t)(0xefd2u+s.cpu.y),&lo) ||
       !cv6_bus_read(&s,rom,n,(uint16_t)(0xefd3u+s.cpu.y),&hi))return cv6_result(0xef91u,0);
    cv5_a(&s,lo);cv6_store(&s,0x08u,s.cpu.a);
    cv5_a(&s,hi);cv6_store(&s,0x09u,s.cpu.a);
    cv5_a(&s,cv6_load(&s,0x05aau));
    cv5_asl_a(&s);s.cpu.p&=(uint8_t)~1u;
    cv6_adc(&s,cv6_load(&s,0x05aau));
    cv5_y(&s,s.cpu.a);
    uint16_t ptr=(uint16_t)(cv6_load(&s,0x08u)|((unsigned)cv6_load(&s,0x09u)<<8));
    uint8_t value;
    if(!cv6_bus_read(&s,rom,n,(uint16_t)(ptr+s.cpu.y),&value))return cv6_result(0xef91u,0);
    cv5_a(&s,value);s.cpu.p&=(uint8_t)~1u;
    cv6_adc(&s,cv6_load(&s,0x0593u));
    cv6_store(&s,0x0400u,s.cpu.a);
    cv5_y(&s,(uint8_t)(s.cpu.y+1u));
    if(!cv6_bus_read(&s,rom,n,(uint16_t)(ptr+s.cpu.y),&value))return cv6_result(0xef91u,0);
    cv5_a(&s,value);cv6_store(&s,0x057cu,s.cpu.a);
    cv5_y(&s,(uint8_t)(s.cpu.y+1u));
    uint8_t index=(uint8_t)(cv6_load(&s,0x0593u)+1u);
    cv6_store(&s,0x0593u,index);cv5_nz(&s,index);
    index=(uint8_t)(index+1u);
    cv6_store(&s,0x0593u,index);cv5_nz(&s,index);
    cv5_a(&s,index);
    if(!cv6_bus_read(&s,rom,n,(uint16_t)(ptr+s.cpu.y),&value))return cv6_result(0xef91u,0);
    cv6_cmp(&s,s.cpu.a,value);
    if(s.cpu.p&1u) {
        cv5_a(&s,0);cv6_store(&s,0x0593u,s.cpu.a);
    }
    *m=s;return cv6_result(0xefd1u,1);
}
/* ROM $86FB entry; all branches into $869E-$8712 are in bank 14.
 * RAM $04DB/$041C arithmetic forms one carry chain in the original code. */
static CviiiCollisionExitV6 cviii_collision_86fb_native_v6(CviiiCollisionMachineV5*m) {
    if(!m)return cv6_result(0,0);
    cv5_a(m,cv6_load(m,0x65u));
    if(!(m->cpu.p&0x80u))goto update;
    cv6_cmp(m,m->cpu.a,0x82u);
    if(!(m->cpu.p&1u))goto state_check;
    cv5_a(m,cv6_load(m,0x65u));
    cv5_a(m,(uint8_t)(m->cpu.a&1u));
    if(m->cpu.p&2u) {
        cv5_a(m,cv6_load(m,0x54u));
        cv6_cmp(m,m->cpu.a,cv6_load(m,0x6eu));
        if(!(m->cpu.p&2u))goto update;
        cv5_a(m,cv6_load(m,0x53u));cv6_cmp(m,m->cpu.a,0x30u);
        if(!(m->cpu.p&2u))goto update;
    } else {
        cv5_a(m,cv6_load(m,0x54u));
        cv5_a(m,(uint8_t)(m->cpu.a|cv6_load(m,0x53u)));
        if(!(m->cpu.p&2u))goto update;
    }
state_check:
    cv5_a(m,cv6_load(m,0x0520u));
    if(m->cpu.p&0x80u) {
        cv5_a(m,cv6_load(m,0x65u));cv5_a(m,(uint8_t)(m->cpu.a&1u));
        if(!(m->cpu.p&2u))goto update;
        cv5_a(m,cv6_load(m,0x041cu));
        if(m->cpu.p&0x80u)goto update;
        cv5_a(m,cv6_load(m,0x53u));cv5_a(m,(uint8_t)(m->cpu.a|cv6_load(m,0x54u)));
        if(m->cpu.p&2u)goto update;
        cv5_a(m,0);cv6_store(m,0x62u,m->cpu.a);
        return cv6_result(0x86fau,1);
    }
    cv5_a(m,(uint8_t)(m->cpu.a|cv6_load(m,0x0537u)));
    if(m->cpu.p&2u)return cv6_result(0x86fau,1);
    cv5_a(m,cv6_load(m,0x65u));cv5_a(m,(uint8_t)(m->cpu.a&1u));
    if(m->cpu.p&2u)goto update;
    cv5_a(m,cv6_load(m,0x041cu));
    if(!(m->cpu.p&0x80u))goto update;
    cv5_a(m,cv6_load(m,0x6eu));cv6_cmp(m,m->cpu.a,cv6_load(m,0x54u));
    if(m->cpu.p&2u) {
        cv5_a(m,cv6_load(m,0x53u));cv6_cmp(m,m->cpu.a,0x30u);
        if(m->cpu.p&2u)goto update;
    }
    cv5_a(m,1);cv6_store(m,0x62u,m->cpu.a);
    return cv6_result(0x86e4u,1);
update:
    cv5_a(m,cv6_load(m,0x04dbu));m->cpu.p&=(uint8_t)~1u;
    cv6_adc(m,cv6_load(m,0x0537u));cv6_store(m,0x04dbu,m->cpu.a);
    cv5_a(m,cv6_load(m,0x041cu));
    cv6_adc(m,cv6_load(m,0x0520u));cv6_store(m,0x041cu,m->cpu.a);
    return cv6_result(0x8712u,1);
}
/* Preserve the original $968A call ordering and its bounded external effects.
 * JSR/RTS stack pushes are represented at the three direct call sites.
 * v4 $8723 remains an instruction-count/timing abstraction; this is NOT
 * a live drop-in replacement.
 * $8ABD resets RAM $0520 and $0537, $EF91 advances a fixed table, then
 * the v5 $8723 path runs, then $86FB applies final conditional state writes. */
static CviiiCollisionExitV6 cviii_collision_968a_native_v6(
    CviiiCollisionMachineV5*m,const uint8_t*rom,size_t len) {
    if(!m||!cv6_rom_check(rom,len))return cv6_result(0x968au,0);
    CviiiCollisionMachineV5 s=*m;
    cv5_jsr(&s,0x968cu); /* $968A JSR: return address - 1 */
    CviiiCollisionExitV6 ex=cviii_collision_8abd_native_v6(&s);
    if(!ex.ok)return cv6_result(0x968au,0);
    cv5_rts(&s);
    cv5_jsr(&s,0x968fu); /* $968D JSR */
    ex=cviii_collision_ef91_native_v6(&s,rom,len);
    if(!ex.ok)return cv6_result(0x968du,0);
    cv5_rts(&s);
    cv5_jsr(&s,0x9692u); /* $9690 JSR */
    CviiiDepExitV4 dep=cviii_collision_8723_integrated_v5(&s,rom,len);
    if(dep.kind!=CVIII_DEP_RTS)return cv6_result(0x9690u,0);
    cv5_rts(&s);
    ex=cviii_collision_86fb_native_v6(&s);
    if(!ex.ok)return cv6_result(0x9693u,0);
    *m=s;return ex;
}

/* -------- Public adapters; existing translated bodies above are unchanged. -------- */
static CollisionExit ce(uint16_t pc, CollisionExitKind kind,unsigned steps,unsigned ok,uint8_t carry) {
    CollisionExit result={pc,kind,steps,(uint8_t)ok,carry};return result;
}
static void from_state_v3(CviiiCollisionCpuV3 *s,const CollisionState *src) {
    s->a=src->a;s->x=src->x;s->y=src->y;s->p=src->p;
    memcpy(s->ram,src->ram,sizeof s->ram);
}
static void to_state_v3(CollisionState *dst,const CviiiCollisionCpuV3 *s) {
    dst->a=s->a;dst->x=s->x;dst->y=s->y;dst->p=s->p;
    memcpy(dst->ram,s->ram,sizeof s->ram);
}
static void from_state_v4(CviiiDepCpuV4 *s,const CollisionState *src) {
    s->a=src->a;s->x=src->x;s->y=src->y;s->p=src->p;
    memcpy(s->ram,src->ram,sizeof s->ram);
}
static void to_state_v4(CollisionState *dst,const CviiiDepCpuV4 *s) {
    dst->a=s->a;dst->x=s->x;dst->y=s->y;dst->p=s->p;
    memcpy(dst->ram,s->ram,sizeof s->ram);
}
static void from_state_v5(CviiiCollisionMachineV5 *m,const CollisionState *src) {
    from_state_v4(&m->cpu,src);m->sp=src->sp;
    m->mapped_prg_bank=src->mapped_prg_bank;
    m->mapper_writes=src->mapper_writes;
}
static void to_state_v5(CollisionState *dst,const CviiiCollisionMachineV5 *m) {
    to_state_v4(dst,&m->cpu);dst->sp=m->sp;
    dst->mapped_prg_bank=m->mapped_prg_bank;
    dst->mapper_writes=m->mapper_writes;
}
CollisionTimedExit Collision_FCDE_Timed(CollisionState *state,
                                          const uint8_t fixed_prg[COLLISION_FIXED_PRG_BYTES]) {
    CollisionTimedExit result={ce(0,COLLISION_UNSUPPORTED,0,0,0),0};
    if(!state||!fixed_prg)return result;
    CviiiCollisionNativeResult r;
    int ok=cviii_collision_fcde_native(fixed_prg,state->ram,state->a,state->x,
                                       state->y,state->sp,state->p,&r);
    if(ok)return result;
    state->a=r.a;state->x=r.x;state->y=r.y;state->sp=r.sp;state->p=r.p;
    result.exit=ce(r.exit_pc,COLLISION_RTS,r.instructions,1,state->p&1u);
    result.cpu_cycles=r.cpu_cycles;
    return result;
}
CollisionExit Collision_FCDE(CollisionState *state,
                             const uint8_t fixed_prg[COLLISION_FIXED_PRG_BYTES]) {
    return Collision_FCDE_Timed(state,fixed_prg).exit;
}
CollisionExit Collision_Caller(uint16_t pc,CollisionState *state) {
    if(!state)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    CviiiCollisionCpuV3 s;from_state_v3(&s,state);
    CviiiCallerResultV3 r=cviii_collision_caller_native_v3(pc,&s);
    if(r.kind==CVIII_NEXT_UNSUPPORTED){
        uint16_t target;
        if(cviii_collision_first_v3(pc,s.p,&target)<0)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
        return ce(target,COLLISION_FALLTHROUGH,0,1,state->p&1u);
    }
    to_state_v3(state,&s);
    CollisionExitKind kind=(r.kind==CVIII_NEXT_COLLISION?COLLISION_QUERY:
          r.kind==CVIII_NEXT_EXTERNAL?COLLISION_JSR:
          r.kind==CVIII_NEXT_RETURN?COLLISION_RTS:
          r.kind==CVIII_NEXT_JUMP?COLLISION_JMP:COLLISION_FALLTHROUGH);
    return ce(r.pc,kind,0,1,state->p&1u);
}
CollisionExit Collision_Dependency(uint16_t pc,CollisionState *state) {
    if(!state)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    CviiiDepCpuV4 s;from_state_v4(&s,state);
    CviiiDepExitV4 r;
    switch(pc) {
        case 0x89eb:r=cviii_collision_89eb_v4(&s);break;
        case 0x9b5e:r=cviii_collision_9b5e_v4(&s);break;
        case 0x8723:r=cviii_collision_8723_v4(&s);break;
        default:return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    }
    if(r.kind==CVIII_DEP_BAD_ENTRY)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    to_state_v4(state,&s);
    return ce(r.pc,r.kind==CVIII_DEP_JSR?COLLISION_JSR:
                   r.kind==CVIII_DEP_JMP?COLLISION_JMP:COLLISION_RTS,
                   0,1,state->p&1u);
}
CollisionExit Collision_Banked(uint16_t pc,CollisionState *state,const uint8_t *rom,size_t len) {
    if(!state||!rom||len!=COLLISION_ROM_BYTES)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    CviiiCollisionMachineV5 m;from_state_v5(&m,state);
    if(pc==0xe648) {
        CviiiCollisionExitV5 r=cviii_collision_e648_v5(&m,rom,len);
        if(!r.ok)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
        to_state_v5(state,&m);
        return ce(r.next_pc,COLLISION_FALLTHROUGH,0,1,r.collision_carry);
    }
    if(pc==0x8723) {
        CviiiDepExitV4 r=cviii_collision_8723_integrated_v5(&m,rom,len);
        if(r.kind==CVIII_DEP_BAD_ENTRY)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
        to_state_v5(state,&m);
        return ce(r.pc,r.kind==CVIII_DEP_RTS?COLLISION_RTS:COLLISION_JSR,
                  0,1,state->p&1u);
    }
    return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
}
CollisionExit Collision_External(uint16_t pc,CollisionState *state,const uint8_t *rom,size_t len) {
    if(!state)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    CviiiCollisionMachineV5 m;from_state_v5(&m,state);
    CviiiCollisionExitV6 r;
    switch(pc) {
        case 0x8abd:r=cviii_collision_8abd_native_v6(&m);break;
        case 0xef91:r=cviii_collision_ef91_native_v6(&m,rom,len);break;
        case 0x86fb:r=cviii_collision_86fb_native_v6(&m);break;
        case 0x968a:r=cviii_collision_968a_native_v6(&m,rom,len);break;
        default:return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    }
    if(!r.ok)return ce(pc,COLLISION_UNSUPPORTED,0,0,0);
    to_state_v5(state,&m);
    return ce(r.next_pc,COLLISION_FALLTHROUGH,0,1,state->p&1u);
}

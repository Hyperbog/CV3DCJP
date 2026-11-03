/* No ROM file required: smoke checks for bounded native interface and fail closed. */
#include "../src/collision/Collision.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    CollisionState *s=(CollisionState *)calloc(1,sizeof *s);
    assert(s);
    /* ROM $8ABD: LDA #0; STA $0520; STA $0537; RTS. */
    s->a=0x91;
    s->ram[0x520]=0x27;
    s->ram[0x537]=0x28;
    CollisionExit out=Collision_External(0x8abd,s,NULL,0);
    assert(out.ok && out.next_pc==0x8ac5);
    assert(s->a==0 && s->ram[0x520]==0 && s->ram[0x537]==0);
    assert(s->p&2u);

    /* Bank 14 $89EB nonnegative path masks $041C high nibble. */
    s->ram[0x65]=0;
    s->ram[0x41c]=0x3f;
    out=Collision_Dependency(0x89eb,s);
    assert(out.ok && out.kind==COLLISION_RTS && s->ram[0x41c]==0x30);
    assert(s->a==0x30 && !(s->p&2u));

    /* Unknown entry and unrepresentable banked call leave state unchanged. */
    CollisionState *before=(CollisionState*)malloc(sizeof *before);
    assert(before);
    memcpy(before,s,sizeof *s);
    out=Collision_External(0x1234,s,NULL,0);
    assert(!out.ok && memcmp(before,s,sizeof *s)==0);
    out=Collision_Banked(0xe648,s,NULL,0);
    assert(!out.ok && memcmp(before,s,sizeof *s)==0);
    out=Collision_FCDE(s,NULL);
    assert(!out.ok && memcmp(before,s,sizeof *s)==0);

    /* ROM-based bounded caller returns the next actual JSR at $834E. */
    s->p=(uint8_t)(s->p & ~2u);
    out=Collision_Caller(0x8345,s); /* branch BNE $8353 */
    assert(out.ok && out.next_pc==0x8353);
    s->p=(uint8_t)(s->p | 2u);
    out=Collision_Caller(0x8345,s);
    assert(out.ok && out.next_pc==0x834e && out.kind==COLLISION_QUERY);

    free(before);
    free(s);
    return 0;
}

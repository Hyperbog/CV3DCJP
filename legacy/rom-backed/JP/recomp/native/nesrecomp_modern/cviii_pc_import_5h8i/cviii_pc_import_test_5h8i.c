#include "cviii_pc_import_5h8i.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static void le16(unsigned char *p,unsigned v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
static void le32(unsigned char *p,uint32_t v){for(int k=0;k<4;k++)p[k]=(unsigned char)(v>>(k*8));}
static void le64(unsigned char *p,uint64_t v){for(int k=0;k<8;k++)p[k]=(unsigned char)(v>>(k*8));}
static uint64_t fnv(const unsigned char *p,size_t n){uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static int write_fixture(unsigned char *buf,int bad) {
    const char *name="cviii_pc_import_test.world";
    memcpy(buf,"CVPCW01\0",8);le16(buf+8,1);le16(buf+10,1);
    le32(buf+12,136);memset(buf+32,0,32);
    const unsigned char sha[32]={
      0x95,0x8a,0x4b,0x5e,0x95,0xe8,0x0a,0xcb,0x2c,0x71,0xa4,0x3e,0x9e,0xf2,0xe9,0x14,
      0x44,0xf3,0xd0,0x97,0xae,0x9d,0x4e,0xeb,0x74,0x7f,0x0d,0xac,0x8a,0xbe,0x7a,0x28};
    memcpy(buf+32,sha,32);
    unsigned char *p=buf+64;
    p[0]=0;p[1]=0;p[2]=0;p[3]=1;
    le16(p+4,4);le16(p+6,4);p[8]=1;
    le64(p+12,0);le64(p+20,0);le32(p+28,104);le32(p+32,32);le32(p+36,0x19000);
    for(int i=0;i<16;i++)le16(buf+104+i*2,1025);
    if(bad)le16(buf+104+12,1); /* wrong tile group */
    le64(buf+16,fnv(buf+64,72));le64(buf+24,fnv(buf+64,72));
    FILE *f=fopen(name,"wb");assert(f);
    assert(fwrite(buf,1,136,f)==136);assert(!fclose(f));return 1;
}
int main(int argc,char **argv){
    if(argc==3 && strcmp(argv[1],"--pack")==0) {
        CviiiPcWorld *loaded=cviii_pc_world_open_5h8i(argv[2]);
        if(!loaded) {fputs("FAIL: imported PC world pack rejected\n",stderr);return 1;}
        printf("PASS: ROM-free PC world loader: %zu rooms, %zu entities, signature %016llx\n",
            loaded->room_count,loaded->entity_count,
            (unsigned long long)loaded->resource_signature);
        int good=(loaded->room_count==135);
        cviii_pc_free(loaded);
        return good?0:2;
    }
    unsigned char buf[136]={0};
    assert(write_fixture(buf,0));
    CviiiPcWorld *world=cviii_pc_world_open_5h8i("cviii_pc_import_test.world");
    assert(world && world->room_count==1 && world->resource_signature!=0);
    CviiiPcRoom *r=cviii_pc_room(world,0,0,0);assert(r && r->placed);
    uint16_t tile=0;assert(cviii_pc_get_tile(r,CVIII_PC_TERRAIN,1,1,&tile) && tile==1025);
    assert(cviii_pc_change_tile(r,CVIII_PC_TERRAIN,1,1,0,CVIII_PC_STAGE));
    assert(cviii_pc_get_tile(r,CVIII_PC_TERRAIN,1,1,&tile) && tile==0);
    cviii_pc_free(world);
    assert(write_fixture(buf,1));assert(!cviii_pc_world_open_5h8i("cviii_pc_import_test.world"));
    remove("cviii_pc_import_test.world");
    puts("PASS 5H-8I: native pack load, verified base tile IDs, mutations, reject altered group");return 0;
}

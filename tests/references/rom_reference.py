#!/usr/bin/env python3
"""Independent ROM-byte instruction execution vs native state transitions.
The reference DECODEs authentic 6502 bytes; the C under test encodes high-level
state logic. No clock/IRQ parity is claimed. Unsupported bus reads fail closed.
"""
import ctypes as ct, hashlib, json, os, pathlib, random, subprocess, tempfile, sys
ROOT=pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0,str(pathlib.Path(__file__).parent))
from disasm6502 import decode,cfg
ROMPATH=pathlib.Path(os.environ['CV3DCJP_JP_ROM'])
ROM=ROMPATH.read_bytes();SHA=hashlib.sha256(ROM).hexdigest()
assert len(ROM)==393232 and SHA=='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
class Cpu(ct.Structure):
 _fields_=[('a',ct.c_uint8),('x',ct.c_uint8),('y',ct.c_uint8),('p',ct.c_uint8),('ram',ct.c_uint8*2048)]
class Machine(ct.Structure):
 _fields_=[('cpu',Cpu),('sp',ct.c_uint8),('mapped_prg_bank',ct.c_uint8),('mapper_writes',ct.c_uint)]
class Exit(ct.Structure):
 _fields_=[('next_pc',ct.c_uint16),('ok',ct.c_uint8)]
WRAP='''#include "cviii_collision_external_native_v6.h"
CviiiCollisionExitV6 v6_8abd(CviiiCollisionMachineV5 *s) {return cviii_collision_8abd_native_v6(s);}
CviiiCollisionExitV6 v6_ef91(CviiiCollisionMachineV5 *s,const uint8_t*r,size_t n) {return cviii_collision_ef91_native_v6(s,r,n);}
CviiiCollisionExitV6 v6_86fb(CviiiCollisionMachineV5 *s) {return cviii_collision_86fb_native_v6(s);}
CviiiCollisionExitV6 v6_968a(CviiiCollisionMachineV5 *s,const uint8_t*r,size_t n) {return cviii_collision_968a_native_v6(s,r,n);}
void v6_use_legacy(CviiiDepCpuV4*s) {(void)cviii_collision_9b5e_v4(s);}
'''
class Ref:
 def __init__(self,m,pc):
  self.a,self.x,self.y,self.p=m.cpu.a,m.cpu.x,m.cpu.y,m.cpu.p
  self.ram=bytearray(m.cpu.ram);self.sp=m.sp;self.bank=m.mapped_prg_bank;self.writes=m.mapper_writes
  self.pc=pc;self.depth=0;self.cover=set();self.edges=set();self.count=0
 def offset(self,addr):
  if addr>=0xe000:return 0x3e010+addr-0xe000
  if 0x8000<=addr<0xc000:return 16+self.bank*0x4000+addr-0x8000
  raise ValueError(f'PC/unmapped {addr:04x}')
 def read(self,addr):
  addr&=65535
  if addr<0x2000:return self.ram[addr&2047]
  if addr>=0xe000 or 0x8000<=addr<0xc000:return ROM[self.offset(addr)]
  raise ValueError(f'Bus read unmapped {addr:04x}')
 def write(self,addr,v):
  addr&=65535;v&=255
  if addr<0x2000:self.ram[addr&2047]=v
  elif addr==0x8000:self.bank=v;self.writes+=1
  else:raise ValueError(f'Bus write unmapped {addr:04x}')
 def push(self,v):self.write(0x100+self.sp,v);self.sp=(self.sp-1)&255
 def pop(self):self.sp=(self.sp+1)&255;return self.read(0x100+self.sp)
 def nz(self,v):self.p=(self.p&~0x82)|(0x80 if v&128 else 0)|(0 if v else 2)
 def compare(self,a,b):
  d=(a-b)&255;self.p=(self.p&~0x83)|(int(a>=b))|(2 if not d else 0)|(d&128)
 def operand_addr(self,mode,val):
  if mode=='zp':return val
  if mode=='zpx':return (val+self.x)&255
  if mode=='zpy':return (val+self.y)&255
  if mode=='abs':return val
  if mode=='absx':return (val+self.x)&65535
  if mode=='absy':return (val+self.y)&65535
  if mode=='indY':return ((self.read(val)|(self.read((val+1)&255)<<8))+self.y)&65535
  if mode=='indX':
   ptr=(val+self.x)&255;return self.read(ptr)|(self.read((ptr+1)&255)<<8)
  raise ValueError(f'bad addressing {mode}')
 def arg(self,mode,val):return val if mode=='imm' else self.read(self.operand_addr(mode,val))
 def arith(self,v,sub=False):
  b=self.a
  if sub:
   n=b-v-(1-(self.p&1));carry=int(n>=0);overflow=int(bool((b^(n&255))&(b^v)&128))
  else:
   n=b+v+(self.p&1);carry=int(n>255);overflow=int(bool((~(b^v))&(b^(n&255))&128))
  self.p=(self.p&~0x41)|carry|(overflow<<6);self.a=n&255;self.nz(self.a)
 def step(self):
  pc=self.pc;ins=decode(ROM,pc,self.offset(pc));m,mode,val=ins.mnemonic,ins.mode,ins.operand
  self.cover.add((15 if pc>=0xe000 else self.bank,pc));nxt=(pc+ins.size)&65535
  if m in ('LDA','LDX','LDY'):
   v=self.arg(mode,val);setattr(self,m[-1].lower(),v);self.nz(v)
  elif m in ('STA','STX','STY'):self.write(self.operand_addr(mode,val),getattr(self,m[-1].lower()))
  elif m in ('DEC','INC'):
   a=self.operand_addr(mode,val);v=(self.read(a)+(1 if m=='INC' else -1))&255;self.write(a,v);self.nz(v)
  elif m in ('CMP','CPX','CPY'):self.compare(getattr(self,'a' if m=='CMP' else m[-1].lower()),self.arg(mode,val))
  elif m=='ASL':
   assert mode=='acc';self.p=(self.p&~1)|((self.a>>7)&1);self.a=(self.a<<1)&255;self.nz(self.a)
  elif m in ('ADC','SBC'):self.arith(self.arg(mode,val),m=='SBC')
  elif m in ('AND','ORA','EOR'):
   v=self.arg(mode,val);self.a=(self.a&v if m=='AND' else self.a|v if m=='ORA' else self.a^v);self.nz(self.a)
  elif m=='CLC':self.p&=~1
  elif m=='SEC':self.p|=1
  elif m=='TAX':self.x=self.a;self.nz(self.x)
  elif m=='TXA':self.a=self.x;self.nz(self.a)
  elif m=='TAY':self.y=self.a;self.nz(self.y)
  elif m=='INY':self.y=(self.y+1)&255;self.nz(self.y)
  elif m=='DEY':self.y=(self.y-1)&255;self.nz(self.y)
  elif m=='PHA':self.push(self.a)
  elif m=='PLA':self.a=self.pop();self.nz(self.a)
  elif m=='JSR':self.push((pc+2)>>8);self.push((pc+2)&255);nxt=val;self.depth+=1
  elif m=='JMP':
   assert mode=='abs';nxt=val
  elif m=='RTS':
   if not self.depth:self.count+=1;return True
   lo=self.pop();hi=self.pop();nxt=(((hi<<8)|lo)+1)&65535;self.depth-=1
  elif mode=='rel':
   takes={'BNE':not bool(self.p&2),'BEQ':bool(self.p&2),'BCC':not bool(self.p&1),
          'BCS':bool(self.p&1),'BMI':bool(self.p&0x80),'BPL':not bool(self.p&0x80),
          'BVC':not bool(self.p&0x40),'BVS':bool(self.p&0x40)}[m]
   self.edges.add((15 if pc>=0xe000 else self.bank,pc,bool(takes)))
   if takes:nxt=ins.destination
  else:raise ValueError(f'Unsupported instruction {ins.text} at {pc:04X}')
  self.pc=nxt;self.count+=1;return False
 def run(self,limit=1000):
  for _ in range(limit):
   if self.step():return self
  raise AssertionError(f'instruction limit at {self.pc:04x} depth {self.depth}')
 def bytes(self):
  a=Machine();a.cpu.a=self.a;a.cpu.x=self.x;a.cpu.y=self.y;a.cpu.p=self.p
  a.cpu.ram[:]=self.ram;a.sp=self.sp;a.mapped_prg_bank=self.bank;a.mapper_writes=self.writes
  return bytes(a)

def seed(r,i,kind):
 m=Machine();m.cpu.a=r.randrange(256);m.cpu.x=r.randrange(256);m.cpu.y=r.randrange(256);m.cpu.p=r.randrange(256)
 m.sp=r.randrange(256);m.mapped_prg_bank=14
 for j in range(2048):m.cpu.ram[j]=r.randrange(256)
 m.cpu.ram[0x23]=14
 # ROM table indices: 15 16-bit pointers in fixed-bank EFD2..EFEF.
 m.cpu.ram[0x048c]=(i%15)*2
 m.cpu.ram[0x05aa]=i%6
 m.cpu.ram[0x0593]=(i//6)%11
 m.cpu.ram[0x057c]=(1 if i%4!=0 else (0 if i%8==0 else 2))
 m.cpu.ram[0x65]=[0,1,0x80,0x81,0x82,0x83,0xff,0x7f][i%8]
 m.cpu.ram[0x0520]=[0,1,0x80,0xff][i%4]
 m.cpu.ram[0x0537]=[0,1,0x80,0xff][(i//4)%4]
 m.cpu.ram[0x53]=[0,1,0x30,0xff][(i//8)%4]
 m.cpu.ram[0x54]=[0,1,0x30,0xff][(i//12)%4]
 m.cpu.ram[0x6e]=m.cpu.ram[0x54] if i%3==0 else r.randrange(256)
 m.cpu.ram[0x041c]=[0,1,0x80,0xff][(i//2)%4]
 return m

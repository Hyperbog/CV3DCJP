"""Execute original Japanese Castlevania III CPU $FCDE collision query.

This is a strict instruction interpreter for the *verified CFG only*, not an
emulator, physics engine, or proof of caller inputs. Reads the real ROM and
machine RAM; rejects instruction/PC drift. NMOS 6502 decimal mode is disabled
in NES 2A03. Standard 6502 flag semantics are reproduced for encountered opcodes.
"""
from __future__ import annotations
from dataclasses import dataclass
from disasm6502 import decode,cfg
import hashlib
ROM_SHA='958a4b5e95e80acb2c71a43e9ef2e91444f3d097ae9d4eeb747f0dac8abe7a28'
BASE=0x3C010
START=0xFCDE
@dataclass
class Result:
 a:int;x:int;y:int;sp:int;carry:bool;zero:bool;negative:bool;overflow:bool
 steps:int;cycles:int;ram_delta:dict;path:list

def run_collision(rom:bytes,ram:bytes,a:int,x:int,y:int=0,sp:int=0xFD,
                  carry:bool=False,zero:bool=False,negative:bool=False,overflow:bool=False,
                  max_steps:int=256)->Result:
 if len(rom)!=393232 or hashlib.sha256(rom).hexdigest()!=ROM_SHA:raise ValueError('wrong ROM identity')
 if len(ram)!=2048:raise ValueError('RAM snapshot must contain 2048 bytes')
 if not all(0<=v<=255 for v in (a,x,y,sp)):raise ValueError('invalid 6502 register')
 mem=bytearray(65536);mem[:2048]=ram
 # Japanese original fixed 16KB PRG window (CPU $C000-$FFFF).
 mem[0xC000:0x10000]=rom[BASE:0x40010]
 originals=bytes(mem[:2048]);pc=START;step=0;cycles=0;path=[]
 c,z,n,v=int(carry),int(zero),int(negative),int(overflow)
 def nz(val):
  nonlocal z,n
  z=int((val&255)==0);n=(val>>7)&1
 def rd(addr):return mem[addr&0xFFFF]
 def wr(addr,value):
  if not 0<=addr<2048:raise ValueError(f'collision writes unexpected address ${addr:04X}')
  mem[addr]=value&255
 while step<max_steps:
  if not 0xFC7D<=pc<=0xFD4C:raise ValueError(f'control escaped collision code at ${pc:04X}')
  ins=decode(rom,pc,BASE+pc-0xC000)
  path.append(pc);step+=1;mn,mode=ins.mnemonic,ins.mode;raw=ins.operand
  nextpc=(pc+ins.size)&0xFFFF; old_y=y
  # Resolve only addressing modes actually present in the verified collision CFG.
  def val():
   if mode=='imm':return raw
   if mode=='zp':return rd(raw)
   if mode=='abs':return rd(raw)
   if mode=='absy':return rd((raw+y)&0xffff)
   raise ValueError(f'unsupported addressing {mode} at ${pc:04X}')
  if mn=='LDA':a=val();nz(a)
  elif mn=='LDY':y=val();nz(y)
  elif mn=='STA':
   if mode!='zp':raise ValueError('unexpected STA mode')
   wr(raw,a)
  elif mn=='STY':
   if mode!='zp':raise ValueError('unexpected STY mode')
   wr(raw,y)
  elif mn=='PHA':wr(0x100+sp,a);sp=(sp-1)&255
  elif mn=='PLA':sp=(sp+1)&255;a=rd(0x100+sp);nz(a)
  elif mn=='TXA':a=x;nz(a)
  elif mn=='TYA':a=y;nz(a)
  elif mn=='TAY':y=a;nz(y)
  elif mn=='CLC':c=0
  elif mn=='SEC':c=1
  elif mn=='LSR':
   if mode=='acc':c=a&1;a>>=1;nz(a)
   elif mode=='zp':
    t=val();c=t&1;t>>=1;wr(raw,t);nz(t)
   else:raise ValueError('unexpected LSR mode')
  elif mn=='ADC':
   src=val();s=a+src+c;v=int((~(a^src)&(a^s)&128)!=0);c=int(s>255);a=s&255;nz(a)
  elif mn=='SBC':
   src=val();d=a-src-(1-c);v=int(((a^src)&(a^d)&128)!=0);c=int(d>=0);a=d&255;nz(a)
  elif mn=='AND':a=a&val();nz(a)
  elif mn=='ORA':a=a|val();nz(a)
  elif mn=='CMP':
   t=a-val();c=int(t>=0);nz(t&255)
  elif mn in ('BCC','BCS','BMI','BNE','BEQ'):
   take={'BCC':c==0,'BCS':c==1,'BMI':n==1,'BNE':z==0,'BEQ':z==1}[mn]
   if take:nextpc=ins.destination
  elif mn=='JMP':
   if mode!='abs':raise ValueError('unexpected indirect JMP')
   nextpc=ins.destination
  elif mn=='RTS':
   changes={f'0x{i:04x}':{'old':originals[i],'new':mem[i]} for i in range(2048) if originals[i]!=mem[i]}
   return Result(a,x,y,sp,bool(c),bool(z),bool(n),bool(v),step,cycles+6,changes,path)
  else:raise ValueError(f'unexpected opcode {ins.text} at ${pc:04X}')
  # Cycle timing is independent of the translated C path and read from ROM opcodes.
  if mode=='rel':
   taken=(nextpc!=(pc+ins.size)&0xffff)
   clock=2+int(taken)+int(taken and (nextpc>>8)!=(((pc+ins.size)&0xffff)>>8))
  elif mn in ('PHA','PHP'): clock=3
  elif mn in ('PLA','PLP'): clock=4
  elif mn=='JMP': clock=3
  elif mn in ('ASL','LSR','ROL','ROR') and mode=='zp':clock=5
  elif mode=='imp' or mode=='acc' or mode=='imm':clock=2
  elif mode=='zp':clock=3
  elif mode=='abs':clock=4
  elif mode=='absy':clock=4+int((ins.operand&255)+old_y>=256)
  else:raise ValueError(f'unmodeled NMOS timing at {pc:04x} {ins.text}')
  cycles+=clock
  pc=nextpc
 raise ValueError('collision routine exceeded step bound')

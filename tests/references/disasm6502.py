"""Strict official 6502 opcode disassembler; undefined opcodes are never guessed."""
from __future__ import annotations
from dataclasses import dataclass
MODES={
 'imp':(1,''),'acc':(1,'A'),'imm':(2,'#${:02X}'),'zp':(2,'${:02X}'),
 'zpx':(2,'${:02X},X'),'zpy':(2,'${:02X},Y'),
 'indX':(2,'(${:02X},X)'),'indY':(2,'(${:02X}),Y'),
 'abs':(3,'${:04X}'),'absx':(3,'${:04X},X'),'absy':(3,'${:04X},Y'),
 'ind':(3,'(${:04X})'),'rel':(2,'${:04X}')}
TABLE={}
def add(op,mn,mode):
 if op in TABLE:raise ValueError('duplicate opcode')
 TABLE[op]=(mn,mode)
# Official NMOS 6502 opcodes, excluding undocumented instructions.
for mn,base in [('ORA',0x00),('AND',0x20),('EOR',0x40),('ADC',0x60),('STA',0x80),('LDA',0xA0),('CMP',0xC0),('SBC',0xE0)]:
 for delta,mode in [(1,'indX'),(5,'zp'),(9,'imm'),(0x0D,'abs'),(0x11,'indY'),(0x15,'zpx'),(0x19,'absy'),(0x1D,'absx')]:
  if mn=='STA' and mode=='imm':continue
  add(base+delta,mn,mode)
for mn,base in [('ASL',0x00),('ROL',0x20),('LSR',0x40),('ROR',0x60)]:
 for delta,mode in [(6,'zp'),(0x0A,'acc'),(0x0E,'abs'),(0x16,'zpx'),(0x1E,'absx')]: add(base+delta,mn,mode)
for mn,base in [('STX',0x80),('LDX',0xA0)]:
 for delta,mode in ([(6,'zp'),(0x0E,'abs'),(0x16,'zpy')] if mn=='STX' else [(2,'imm'),(6,'zp'),(0x0E,'abs'),(0x16,'zpy'),(0x1E,'absy')]): add(base+delta,mn,mode)
for mn,base in [('STY',0x80),('LDY',0xA0),('CPY',0xC0),('CPX',0xE0)]:
 for delta,mode in ([(4,'zp'),(0x0C,'abs'),(0x14,'zpx')] if mn=='STY' else [(0,'imm'),(4,'zp'),(0x0C,'abs'),(0x14,'zpx'),(0x1C,'absx')] if mn=='LDY' else [(0,'imm'),(4,'zp'),(0x0C,'abs')]): add(base+delta,mn,mode)
for mn,base in [('DEC',0xC0),('INC',0xE0)]:
 for delta,mode in [(6,'zp'),(0x0E,'abs'),(0x16,'zpx'),(0x1E,'absx')]: add(base+delta,mn,mode)
for op,mn,mode in [
(0x00,'BRK','imp'),(0x08,'PHP','imp'),(0x10,'BPL','rel'),(0x18,'CLC','imp'),(0x20,'JSR','abs'),(0x24,'BIT','zp'),(0x28,'PLP','imp'),(0x2C,'BIT','abs'),(0x30,'BMI','rel'),(0x38,'SEC','imp'),(0x40,'RTI','imp'),(0x48,'PHA','imp'),(0x4C,'JMP','abs'),(0x50,'BVC','rel'),(0x58,'CLI','imp'),(0x60,'RTS','imp'),(0x68,'PLA','imp'),(0x6C,'JMP','ind'),(0x70,'BVS','rel'),(0x78,'SEI','imp'),(0x88,'DEY','imp'),(0x8A,'TXA','imp'),(0x90,'BCC','rel'),(0x98,'TYA','imp'),(0x9A,'TXS','imp'),(0xA8,'TAY','imp'),(0xAA,'TAX','imp'),(0xB0,'BCS','rel'),(0xB8,'CLV','imp'),(0xBA,'TSX','imp'),(0xC8,'INY','imp'),(0xCA,'DEX','imp'),(0xD0,'BNE','rel'),(0xD8,'CLD','imp'),(0xE8,'INX','imp'),(0xEA,'NOP','imp'),(0xF0,'BEQ','rel'),(0xF8,'SED','imp')]: add(op,mn,mode)
@dataclass(frozen=True)
class Instruction:
 pc:int;file_offset:int;bytes_hex:str;mnemonic:str;mode:str;operand:int|None;destination:int|None;text:str;size:int

def decode(rom:bytes, pc:int, file_offset:int)->Instruction:
 code=rom[file_offset]
 if code not in TABLE: raise ValueError(f'UNDEFINED opcode ${code:02X} at CPU ${pc:04X} file ${file_offset:05X}')
 mn,mode=TABLE[code];size,fmt=MODES[mode]
 bs=rom[file_offset:file_offset+size]
 if len(bs)!=size:raise ValueError('truncated')
 raw = bs[1] if size==2 else (bs[1]|bs[2]<<8 if size==3 else None)
 val = ((pc+size+(raw-256 if raw>=128 else raw))&65535) if mode=='rel' else raw
 arg=fmt.format(val) if size>1 else fmt
 dest=val if mode=='rel' or mn in ('JSR','JMP') and mode=='abs' else None
 return Instruction(pc,file_offset,bs.hex(' '),mn,mode,raw,dest,mn+(' '+arg if arg else ''),size)

def disasm(rom:bytes, start:int,end:int,base:int=0xE000,rom_base:int=0x3E010)->list[Instruction]:
 i=start;out=[]
 while i<end:
  ins=decode(rom,i,rom_base+i-base);out.append(ins);i+=ins.size
 return out

def cfg(rom:bytes, start:int, address_min:int, address_max:int,
        *,cpu_base:int=0xE000,rom_base:int=0x3E010,max_instructions:int=1500):
 """Within fixed mapping only. JSR targets listed but not followed; indirect JMP unresolved."""
 todo=[start]; instructions={}; errors=[]; calls=[]; indirect=[]
 while todo:
  pc=todo.pop()
  if pc in instructions:continue
  if not address_min<=pc<address_max:
   errors.append({'from':start,'target':pc,'reason':'out-of-scope'});continue
  try:ins=decode(rom,pc,rom_base+pc-cpu_base)
  except (ValueError,IndexError) as exc:errors.append({'at':pc,'error':str(exc)});continue
  instructions[pc]=ins
  if len(instructions)>max_instructions:raise ValueError('CFG expansion limit')
  nxt=(pc+ins.size)&0xFFFF
  if ins.mnemonic in ('RTS','RTI','BRK'):continue
  if ins.mnemonic=='JSR':
   calls.append({'caller_pc':pc,'target_pc':ins.destination});todo.append(nxt)
  elif ins.mnemonic=='JMP':
   if ins.mode=='ind':indirect.append({'pc':pc,'pointer_pc':ins.operand})
   else:todo.append(ins.destination)
  elif ins.mode=='rel':todo.extend([nxt,ins.destination])
  else:todo.append(nxt)
 return sorted(instructions.values(),key=lambda v:v.pc),errors,calls,indirect

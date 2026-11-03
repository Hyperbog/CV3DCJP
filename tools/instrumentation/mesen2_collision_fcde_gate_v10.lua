-- Read-only Mesen2 $FCDE entry/return trace. No CPU/PPU writes.
-- Target: Japanese Akumajou Densetsu, original ROM SHA1 below.
-- Mesen2: enable "Allow access to I/O and OS functions" before running.
local expected='a0f3b31d4e3b0d2ca2e8a34f91f14ad99a5ad11f'
local info=assert(emu.getRomInfo(),'No game loaded')
assert(type(info.fileSha1Hash)=='string' and info.fileSha1Hash:lower()==expected,
       'Wrong ROM; refusing collision capture')
local dir=assert(emu.getScriptDataFolder(),'Script-data directory unavailable')
local sep=package.config:sub(1,1)
local path=dir..sep..'cv3dcjp-fcde-v10-'..os.date('%Y%m%d-%H%M%S')..'.csv'
local check=io.open(path,'rb')
assert(not check,'Capture file already exists; retry with a new timestamp')
local out=assert(io.open(path,'wb'),'Cannot create capture output (I/O permission)')
out:write('caller_pc,entry_cycle,return_cycle,entry_a,entry_x,entry_y,entry_sp,entry_p,return_a,return_x,return_y,return_sp,return_p,entry_ram_hex,return_ram_hex\n')
out:flush()
local MEM=emu.memType;local CB=emu.callbackType
local RAM=assert(MEM.nesInternalRam)
local CPU_DEBUG=assert(MEM.nesDebug)
local PRG=assert(MEM.nesPrgRom)
local function read(a,t)return emu.read(a,t) end
local function regs()
  local s=emu.getState()
  return {a=assert(s['cpu.a']),x=assert(s['cpu.x']),y=assert(s['cpu.y']),
          sp=assert(s['cpu.sp']),p=assert(s['cpu.ps']),
          cycle=assert(s['cpu.cycleCount'])}
end
local function physical(address)
  local c=emu.convertAddress(address,MEM.nesMemory)
  if type(c)~='table' or c.memType~=PRG then return -1 end
  return c.address
end
local function hexram()
  local t={}
  for i=0,2047 do t[#t+1]=string.format('%02x',read(i,RAM)) end
  return table.concat(t)
end
local callers={
  0x8345,0x834e,0x839d,0x83ea,0x83f6,0x8429,0x8450,0x84cf,0x84d8,
  0x9153,0x9164,0x91cf,0x91d8,0x9212,0x96cf,0x96d8,0x96e4,
  0x96ed,0x970b,0x9714,0x9720,0x9729,0x97b1,0x97ba,0x97c5,
  0x97ce,0x97e7,0x97fc,0x9b3c,0x9b45,0x9b4e,0x9b59,0x9b7b,
  0x9b84,0x9b8d,0x9b96
}
local allow, pending={},{}
for _,addr in ipairs(callers) do allow[addr]=true end
local MAX=128;local count=0
local function entry(pc,opcode)
  if count>=MAX or pc~=0xfcde or physical(pc)~=0x3fcde then return end
  local s=regs(); local sp=s.sp
  local ret=read(0x100+((sp+1)&255),RAM)+256*read(0x100+((sp+2)&255),RAM)
  local caller=(ret-2)&65535
  if not allow[caller] then return end
  if physical(caller)~=0x38000+caller-0x8000 then return end
  if read(caller,CPU_DEBUG)~=0x20 or read(caller+1,CPU_DEBUG)~=0xde
     or read(caller+2,CPU_DEBUG)~=0xfc then return end
  local return_pc=(caller+3)&65535
  if pending[return_pc] then return end
  pending[return_pc]={caller=caller,first=s,ram=hexram()}
end
local ok,why=pcall(emu.addMemoryCallback,entry,CB.exec,0xfcde,0xfcde,
                   emu.cpuType.nes,MEM.nesMemory)
assert(ok,'Mesen2 $FCDE exec callback unavailable: '..tostring(why))
for _,caller in ipairs(callers) do
  local ret=(caller+3)&65535
  local function on_return(pc,opcode)
    local hit=pending[ret]
    if not hit or count>=MAX then return end
    pending[ret]=nil
    if physical(pc)~=0x38000+pc-0x8000 then return end
    local s=regs()
    if s.cycle<hit.first.cycle then return end
    local f=hit.first
    local row={hit.caller,f.cycle,s.cycle,f.a,f.x,f.y,f.sp,f.p,
               s.a,s.x,s.y,s.sp,s.p,hit.ram,hexram()}
    out:write(table.concat(row,',')..'\n')
    out:flush();count=count+1
    if count==MAX then emu.log('Collision v10 capture full: '..path) end
  end
  local worked,error_text=pcall(emu.addMemoryCallback,on_return,CB.exec,ret,ret,
                               emu.cpuType.nes,MEM.nesMemory)
  assert(worked,'Mesen2 return callback unavailable: '..tostring(error_text))
end
emu.addEventCallback(function()out:flush();out:close() end,emu.eventType.scriptEnded)
emu.log('Collision v10 read-only FCDE trace active: '..path)

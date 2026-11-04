-- v11.2 read-only paired-input/DMC/FCDE capture; no machine writes.
-- Original Japanese ROM required. Reset AFTER starting script to log
-- the full controller history from boot. MesenCE APIs source-verified.
-- Uses the documented getCpuCycleCount API, not unstable getState cycle fields.
-- GUI script: play the original game; then end the script to close the CSV.
-- Target: Japanese Akumajou Densetsu, original ROM SHA1 below.
-- Mesen2: enable "Allow access to I/O and OS functions" before running.
local expected='a0f3b31d4e3b0d2ca2e8a34f91f14ad99a5ad11f'
local info=assert(emu.getRomInfo(),'No game loaded')
assert(type(info.fileSha1Hash)=='string' and info.fileSha1Hash:lower()==expected,
       'Wrong ROM; refusing collision capture')
local dir=emu.getScriptDataFolder()
assert(type(dir)=='string' and #dir>0,
       'Enable Mesen Lua access to I/O and OS functions to write capture')
local sep=package.config:sub(1,1)
local stamp=os.date('%Y%m%d-%H%M%S')
local path=dir..sep..'cv3dcjp-fcde-sync-v11-'..stamp..'.csv'
local dmc_path=dir..sep..'cv3dcjp-fcde-dmc-v11-'..stamp..'.csv'
local input_path=dir..sep..'cv3dcjp-controller-v11-'..stamp..'.csv'
local check=io.open(path,'rb')
assert(not check,'Capture file already exists; retry with a new timestamp')
for _,name in ipairs({dmc_path,input_path}) do
  local existing=io.open(name,'rb')
  assert(not existing,'A synchronized v11 capture filename already exists: '..name)
end
local out=assert(io.open(path,'wb'),'Cannot create capture output (I/O permission)')
local dmc_out=assert(io.open(dmc_path,'wb'),'Cannot open DMC state output')
local input_out=assert(io.open(input_path,'wb'),'Cannot open input state output')
dmc_out:write('sample,caller_pc,entry_cycle,return_cycle,entry_dmc_address,return_dmc_address,entry_dmc_remaining,return_dmc_remaining,entry_buffer_empty,return_buffer_empty\n')
input_out:write('reset_epoch,mesen_frame,cycle,port1_mask,port2_mask\n')
dmc_out:flush();input_out:flush()
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
          cycle=assert(emu.getCpuCycleCount(emu.cpuType.nes))}
end
-- Verified from MesenCE Core/NES/APU/DeltaModulationChannel.cpp
-- Serialize() plus the bundled NesDmcCapture.lua example.
local function dmc_state()
  local s=emu.getState()
  local addr=assert(s['apu.dmc.currentAddr'],'MesenCE DMC currentAddr unavailable')
  local remain=assert(s['apu.dmc.bytesRemaining'],'MesenCE DMC bytesRemaining unavailable')
  local empty=s['apu.dmc.bufferEmpty']
  assert(type(empty)=='boolean','MesenCE DMC bufferEmpty unavailable')
  return {addr=addr,remaining=remain,empty=empty and 1 or 0}
end

-- Verified from MesenCE NesController.h GetKeyNameAssociations() and
-- LuaApi.cpp GetInput(). A missing port is an empty table; unsupported
-- nonstandard controller layouts are rejected rather than guessed.
local button_names={'a','b','select','start','up','down','left','right'}
local function pad_mask(port)
  local buttons=assert(emu.getInput(port),'Controller state unavailable')
  local populated=false
  for _,name in ipairs(button_names) do
    if buttons[name]~=nil then populated=true;break end
  end
  if not populated then return 0 end
  local mask=0
  for bit,name in ipairs(button_names) do
    assert(type(buttons[name])=='boolean','Unsupported controller button: '..name)
    if buttons[name] then mask=mask|(1<<(bit-1)) end
  end
  return mask
end

local epoch=0
emu.addEventCallback(function() epoch=epoch+1 end,emu.eventType.reset)
emu.addEventCallback(function()
  local state=emu.getState()
  local frame=assert(state['frameCount'],'MesenCE frameCount unavailable')
  input_out:write(string.format('%d,%d,%d,%d,%d\n',epoch,frame,
                  emu.getCpuCycleCount(emu.cpuType.nes),
                  pad_mask(0),pad_mask(1)))
end,emu.eventType.inputPolled)
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
local MAX=128;local count=0;local skipped=0
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
  if pending[return_pc] then skipped=skipped+1;return end
  pending[return_pc]={caller=caller,first=s,ram=hexram(),dmc=dmc_state()}
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
    out:flush()
    local last=dmc_state()
    dmc_out:write(string.format('%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n',
       count+1,hit.caller,f.cycle,s.cycle,
       hit.dmc.addr,last.addr,hit.dmc.remaining,last.remaining,
       hit.dmc.empty,last.empty))
    dmc_out:flush()
    count=count+1
    if count==MAX then emu.log('Collision v11 capture full: '..path) end
  end
  local worked,error_text=pcall(emu.addMemoryCallback,on_return,CB.exec,ret,ret,
                               emu.cpuType.nes,MEM.nesMemory)
  assert(worked,'Mesen2 return callback unavailable: '..tostring(error_text))
end
emu.addEventCallback(function()
  out:flush();out:close()
  dmc_out:flush();dmc_out:close()
  input_out:flush();input_out:close()
  emu.log('Collision v11 trace closed: '..count..' rows, '..skipped..' overlaps: '..path)
end,emu.eventType.scriptEnded)
emu.log('Collision v11.2 collision CPU trace: '..path)
emu.log('Collision v11.2 DMC boundary state: '..dmc_path)
emu.log('Collision v11.2 controller history: '..input_path)
emu.log('Reset the original game AFTER starting capture; reset_epoch=0 is not boot-complete.')

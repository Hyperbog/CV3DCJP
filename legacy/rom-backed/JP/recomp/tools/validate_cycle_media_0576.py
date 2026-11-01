#!/usr/bin/env python3
"""0576 media policy for our external NESRecomp CMake wrapper.

Default accepts only the authenticated Japanese retail ROM. A separately
armed CI-only option accepts ONE exact, locally invented mapper24 fixture.
"""
from __future__ import annotations
import argparse,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[2] / 'decomp' / 'tools'))
from nesrecomp_cycle_0568 import authenticated
from test_cycle_synthetic_rom_0569 import create,verify

def validate(rom:Path, allow_synthetic:bool=False)->dict:
    data=rom.read_bytes()
    if allow_synthetic and data==create():
        info=verify(data)
        info["accepted_media"]="synthetic-reproducible-CI-fixture"
        return info
    info=authenticated(rom)
    info["accepted_media"]="authenticated-original-Japanese-retail-ROM"
    return info

def smoke()->None:
    import tempfile
    with tempfile.TemporaryDirectory(prefix="jp0576-") as tmp:
        f=Path(tmp)/"synthetic.nes"
        data=create()
        f.write_bytes(data)
        assert validate(f,True)["accepted_media"]=="synthetic-reproducible-CI-fixture"
        try:validate(f)
        except ValueError:pass
        else:raise AssertionError("synthetic cartridge accepted in retail-only default")
        bad=bytearray(data);bad[16+31*8192+0x80]^=1
        f.write_bytes(bad)
        try:validate(f,True)
        except ValueError:pass
        else:raise AssertionError("modified synthetic cartridge accepted as original")
    print("PASS JP0576 exact synthetic fixture ONLY by explicit CI option; retail default strict")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom",type=Path)
    p.add_argument("--allow-synthetic",action="store_true")
    p.add_argument("--smoke",action="store_true")
    a=p.parse_args()
    if a.smoke:smoke();return
    if not a.rom:p.error("--rom required")
    print(json.dumps(validate(a.rom,a.allow_synthetic),sort_keys=True))
if __name__=="__main__":
    try:main()
    except (OSError,ValueError,AssertionError) as e:
        print("JP0576 media rejection: "+str(e),file=sys.stderr)
        raise SystemExit(2)

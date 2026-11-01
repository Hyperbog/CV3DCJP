#!/usr/bin/env python3
"""0575 one-command, externally pinned native C11 build for original JP mapper24.
No upstream source or retail-ROM-derived code is checked in.
"""
from __future__ import annotations
import argparse,json,shutil,subprocess,sys
from pathlib import Path
from types import SimpleNamespace
sys.path.insert(0,str(Path(__file__).resolve().parents[2] / 'decomp' / 'tools'))
from nesrecomp_cycle_0568 import authenticated,run,validate_engine

ROOT=Path(__file__).resolve().parents[3].resolve()
PIN="2b4fcdb98c9e702be3fb365cfc645003c50b5fc1"
UPSTREAM="https://github.com/mstan/nesrecomp.git"

def output_directory(path:Path)->Path:
    out=path.resolve()
    if out==ROOT or ROOT in out.parents:
        raise ValueError("ROM-derived output must be outside the CVIII repository")
    return out

def revision(engine:Path)->str:
    p=subprocess.run(["git","-C",str(engine),"rev-parse","HEAD"],
                     capture_output=True,text=True,check=True)
    return p.stdout.strip()

def pinned_engine(folder:Path,offline:bool)->Path:
    engine=folder/"_external"/"nesrecomp"
    if engine.is_dir():
        if revision(engine)!=PIN:
            raise ValueError("Existing external engine revision differs from pinned revision")
    else:
        if offline:raise ValueError("Offline mode requires an already pinned engine")
        engine.parent.mkdir(parents=True,exist_ok=True)
        subprocess.run(["git","clone","--no-checkout",UPSTREAM,str(engine)],check=True)
        subprocess.run(["git","-C",str(engine),"checkout","--detach",PIN],check=True)
        if revision(engine)!=PIN:raise ValueError("Pinned external checkout drift")
    validate_engine(engine)
    return engine

def plan(rom:Path,out:Path,frames:int,windowed:bool,
         ppu_impl:str="LLE")->dict:
    if frames<1 or frames>12000:raise ValueError("frames must be 1..12000")
    if ppu_impl not in ("LLE","HLE"):raise ValueError("ppu_impl must be LLE or HLE")
    out=output_directory(out)
    return dict(schema="JP0575-retail-C11-build/v1",
                rom=authenticated(rom.resolve(strict=True)),upstream=UPSTREAM,
                pinned_sha=PIN,engine=str(out/"_external"/"nesrecomp"),
                build_dir=str(out/"build"),frames=frames,headless=not windowed,
                ppu_implementation=ppu_impl,
                real_game_compiled=False,gameplay_parity_verified=False)

def build(rom:Path,out:Path,frames:int,windowed:bool,offline:bool,
          build_only:bool=False,input_script:Path|None=None,
          ppu_impl:str="LLE")->dict:
    info=plan(rom,out,frames,windowed,ppu_impl)
    folder=output_directory(out);folder.mkdir(parents=True,exist_ok=True)
    engine=pinned_engine(folder,offline)
    args=SimpleNamespace(rom=rom.resolve(strict=True),engine=engine,
                         out=folder/"build",frames=frames,build_only=build_only,
                         input_script=input_script,windowed=windowed,
                         ppu_impl=ppu_impl,plan=False)
    result=run(args)
    result["external_engine_sha"]=PIN
    (folder/"jp0575_build_report.json").write_text(
        json.dumps(result,indent=2,sort_keys=True)+"\n",encoding="utf-8")
    return result

def smoke():
    assert len(PIN)==40 and all(c in "0123456789abcdef" for c in PIN)
    try:output_directory(ROOT/"JP"/"recomp"/"native"/"retail-build")
    except ValueError:pass
    else:raise AssertionError("Private build output allowed in public repo")
    from nesrecomp_cycle_0568 import commands
    for n in (0,12001):
        try:commands(Path("/a"),Path("/b"),Path("/c"),n,True)
        except ValueError:pass
        else:raise AssertionError("Unbounded original-game frames allowed")
    print("PASS JP0575 pinned external C11 build safety contract")

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--rom",type=Path);p.add_argument("--out",type=Path)
    p.add_argument("--frames",type=int,default=120)
    p.add_argument("--windowed",action="store_true")
    p.add_argument("--ppu-impl",choices=("LLE","HLE"),default="LLE",
                   help="Original: LLE; HLE is a diagnostic A/B backend")
    p.add_argument("--offline",action="store_true")
    p.add_argument("--build-only",action="store_true",
                   help="Build original game into native C11 executable without running")
    p.add_argument("--input-script",type=Path,
                   help="Optional deterministic frame/button schedule, no recording required")
    p.add_argument("--plan",action="store_true")
    p.add_argument("--smoke",action="store_true")
    a=p.parse_args()
    if a.smoke:smoke();return
    if not a.rom or not a.out:p.error("--rom and --out required")
    if not shutil.which("git") or not shutil.which("cmake"):
        raise ValueError("git and cmake are required")
    if a.plan:
        print(json.dumps(plan(a.rom,a.out,a.frames,a.windowed,a.ppu_impl),
                         indent=2,sort_keys=True))
    else:
        print(json.dumps(build(a.rom,a.out,a.frames,a.windowed,a.offline,
                              a.build_only,a.input_script,a.ppu_impl),
                         indent=2,sort_keys=True))
if __name__=="__main__":
    try:main()
    except (ValueError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as e:
        print("JP0575 ERROR: "+str(e),file=sys.stderr);raise SystemExit(2)

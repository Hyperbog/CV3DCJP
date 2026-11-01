#!/usr/bin/env python3
"""Import authenticated pre-W8 source without ROMs, generated assets or binaries."""
import argparse
import hashlib
import json
import pathlib
import sys
import tempfile
import zipfile

HOST="JP/recomp/native/nesrecomp_modern/"
PREFIX="legacy/rom-backed/"
ANCHORS={
 "CMakeLists.txt":"beff0283313293b2984d62910935293eff92e7c8dc54e4a1dca204995497abcd",
 "cviii_modern_host.c":"fa1221b9349b2308e407efdcb892944a26e1a8d7b7719617b3faeb7dd4109a51",
 "cviii_room_diag.c":"1313e492f8b3ee8a7b89be77c8ec91865f2fd226805296b5791aa62d594d8b7e",
}
LAUNCHER="e1a92598a135a8cfb9da1b3680d560f137e76b1ca3ac2fd5ad225727084258fe"
ALLOWED={".c",".h",".cpp",".hpp",".py",".txt",".md",".ps1"}
REQUIRED=[
 "JP/recomp/native/enhanced/cviii_audio_levels_0618.c",
 "JP/recomp/native/enhanced/cviii_sprite_layers_0608.c",
 "JP/recomp/native/enhanced/cviii_chr_index_0598.c",
 "JP/recomp/native/enhanced/cviii_whole_room.c",
 "JP/recomp/native/enhanced/cviii_room_gfx.c",
 "JP/recomp/native/enhanced/cviii_room_renderer.c",
 "JP/recomp/native/enhanced/cviii_room_preview.c",
 "JP/recomp/tools/cviii_asset_registry_5h1.py",
 "JP/recomp/tools/cviii_build_world_index_5h2.py",
 "JP/recomp/tools/jp_asset_pack_0597.py",
 "JP/recomp/tools/jp_asset_runtime_0598.py",
 "JP/recomp/tools/build_room_resources.py",
 "JP/recomp/tools/build_room_graphics.py",
 "JP/recomp/tests/test_cviii_asset_registry_5h1.py",
 "JP/recomp/tests/test_cviii_world_index_5h2.py",
]
def sha(data): return hashlib.sha256(data).hexdigest()
def safe_path(value):
    p=pathlib.PurePosixPath(value.replace("\\","/"))
    if p.is_absolute() or ".." in p.parts or ":" in value or not p.parts:
        raise ValueError("unsafe path: "+value)
    return p
def source(data,path):
    if not data or len(data)>1048576 or b"\0" in data:
        raise ValueError("binary or oversized source: "+path)
    data.decode("utf-8")
    return data
def collect(snapshot,workspace):
    entries={}
    with zipfile.ZipFile(snapshot) as z:
        for info in z.infolist():
            if info.is_dir(): continue
            name=info.filename.replace("\\","/")
            if name.startswith(PREFIX+HOST):
                relative=name[len(PREFIX+HOST):]
            elif name==PREFIX+"Build-CVIII-Phase5H8F.ps1":
                content=source(z.read(info),name)
                if sha(content)!=LAUNCHER: raise ValueError("launcher identity mismatch")
                entries["Build-CVIII-Phase5H8F.ps1"]=content
                continue
            elif name.startswith(PREFIX): continue
            else: relative=name
            p=safe_path(relative)
            if p.name in {"README.md","SOURCE_MANIFEST.json"}: continue
            if p.suffix.lower() not in ALLOWED and p.name!="CMakeLists.txt":
                raise ValueError("unexpected archive member: "+name)
            key=HOST+str(p)
            if key in entries: raise ValueError("duplicate archive member: "+key)
            entries[key]=source(z.read(info),key)
    for name,expected in ANCHORS.items():
        if sha(entries.get(HOST+name,b""))!=expected:
            raise ValueError("not the authenticated pre-W8 source: "+name)
    missing=[p for p in REQUIRED if not (workspace/p).is_file()]
    if missing: raise FileNotFoundError("missing owner-local source:\n  "+"\n  ".join(missing))
    # Dependencies are already pinned from source revision f7f533b in GitHub.
    # Never copy possibly modified W13 workspace modules over that baseline.
    if "Build-CVIII-Phase5H8F.ps1" not in entries:
        launcher=workspace/"Build-CVIII-Phase5H8F.ps1"
        if not launcher.is_file() or sha(launcher.read_bytes())!=LAUNCHER:
            raise FileNotFoundError("archived 5H8F launcher not found")
        entries["Build-CVIII-Phase5H8F.ps1"]=source(launcher.read_bytes(),str(launcher))
    return entries
def stage(snapshot,workspace,dest):
    entries=collect(snapshot,workspace)
    meta=json.dumps({
        "status":"HOST SOURCE IMPORTED; WINDOWS/ROM GAMEPLAY NOT YET VERIFIED",
        "host_source":"authenticated pre-W8 5H8F snapshot",
        "dependencies":"pinned Hyperbog/CVIII-Decomp f7f533b",
        "sources":[{"path":k,"sha256":sha(v),"bytes":len(v)}
                   for k,v in sorted(entries.items())]
    },indent=2)+"\n"
    entries["HOST_SOURCE_MANIFEST.json"]=meta.encode("utf-8")
    targets=[dest.joinpath(*safe_path(name).parts) for name in entries]
    if dest.exists() and not dest.is_dir():
        raise FileExistsError("destination is not a directory: "+str(dest))
    conflicts=[str(p) for p in targets if p.exists()]
    if conflicts:
        raise FileExistsError("refusing to overwrite existing sources:\n  "+"\n  ".join(conflicts))
    written=[]
    try:
        for name,data in entries.items():
            path=dest.joinpath(*safe_path(name).parts)
            path.parent.mkdir(parents=True,exist_ok=True)
            with path.open("xb") as output:
                output.write(data)
            written.append(path)
    except BaseException:
        for path in reversed(written):
            path.unlink(missing_ok=True)
        raise
    return len(entries)-1

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--snapshot",required=True,type=pathlib.Path)
    p.add_argument("--workspace",required=True,type=pathlib.Path)
    p.add_argument("--destination",required=True,type=pathlib.Path)
    args=p.parse_args()
    print("STAGED: %d source files"%stage(args.snapshot,args.workspace,args.destination))
if __name__=="__main__":
    try: main()
    except (OSError,ValueError,zipfile.BadZipFile,UnicodeError) as e:
        print("IMPORT BLOCKED: "+str(e),file=sys.stderr)
        sys.exit(1)

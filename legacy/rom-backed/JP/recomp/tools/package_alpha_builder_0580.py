#!/usr/bin/env python3
"""Create a public ROM-free Alpha Windows build/launch kit; no executable."""
from __future__ import annotations
import argparse,hashlib,json,tempfile,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
NAMES=("Build-Alpha.ps1","Launch-Alpha.ps1","README_ALPHA.md",
       "Play-CVIII.bat",
       "Debug-Stage-Companion.bat","Capture-QA-0602.ps1",
       "Build-And-Play.ps1","Build-And-Play.bat","Resolve-Python.ps1",
       "Control-Profiles.py","Controls.ps1","Controls.bat")
def make(out:Path)->dict:
    out=out.resolve()
    if out.suffix.lower()!=".zip":raise ValueError("Require ZIP output")
    out.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(out,"w",zipfile.ZIP_DEFLATED) as z:
        for name in NAMES:z.write(ROOT/"JP/recomp/release/alpha"/name,arcname=name)
    with zipfile.ZipFile(out) as z:
        names=z.namelist()
        if sorted(names)!=sorted(NAMES):raise ValueError("Unexpected alpha kit file")
        if any(x.lower().endswith((".nes",".exe",".dll",".sav")) for x in names):
            raise ValueError("Refusing private game output in public artifact")
    return {"version":"0.2.0-alpha.1","kind":"build-kit-not-playable",
            "path":str(out),"sha256":hashlib.sha256(out.read_bytes()).hexdigest(),
            "includes_game_binary":False,"includes_commercial_rom":False}
def main():
    p=argparse.ArgumentParser()
    p.add_argument("--out",type=Path)
    p.add_argument("--smoke",action="store_true")
    a=p.parse_args()
    if a.smoke:
        with tempfile.TemporaryDirectory(prefix="jp0580-") as temp:
            report=make(Path(temp)/"kit.zip")
            assert not report["includes_game_binary"]
            with zipfile.ZipFile(report["path"]) as kit:
                assert "Build-And-Play.bat" in kit.namelist()
                assert "Build-And-Play.ps1" in kit.namelist()
                assert "Capture-QA-0602.ps1" in kit.namelist()
                assert "Resolve-Python.ps1" in kit.namelist()
                assert "Control-Profiles.py" in kit.namelist()
                assert "Controls.ps1" in kit.namelist()
                assert "Controls.bat" in kit.namelist()
                assert b"cviii-native-keyboard-profile/v1" in kit.read("Control-Profiles.py")
                assert b"Test-CviiiPython" in kit.read("Resolve-Python.ps1")
                assert "Play-Original.bat" not in kit.namelist()
                assert "Play-Enhanced.bat" not in kit.namelist()
                launcher=kit.read("Play-CVIII.bat")
                assert b"Build-And-Play.ps1" in launcher and b"-Edition Enhanced" in launcher
                assert not any(name.lower().endswith((".exe",".nes",".dll")) for name in kit.namelist())
            print("PASS first-run build-and-play ROM-free alpha kit contract")
    elif a.out:print(json.dumps(make(a.out),sort_keys=True))
    else:p.error("--smoke or --out is required")
if __name__=="__main__":main()

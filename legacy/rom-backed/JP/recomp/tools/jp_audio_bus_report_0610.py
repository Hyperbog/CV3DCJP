#!/usr/bin/env python3
"""JP0610: diagnose 2A03/VRC6 register contention in private native play.

This is hardware evidence, NOT separated music or SFX. The Japanese sound
scheduler's source ownership must be proved before duplicating voices.
"""
from __future__ import annotations
import argparse
import csv
import io
import json
from collections import Counter
from pathlib import Path

FIELDS=("cycle","chip","voice","register","value","previous","delta_cycles","rapid_rewrite")
VOICES_2A03=("pulse1","pulse2","triangle","noise","dmc")
VOICES_VRC6=("pulse1","pulse2","saw")

def expected(addr: int) -> tuple[str,str] | None:
    if 0x4000 <= addr <= 0x4013:
        return "2a03", VOICES_2A03[(addr-0x4000)//4]
    if addr in (0x4015,0x4017):
        return "2a03","control"
    if (addr>>12) in (9,10,11) and (addr&15)<=(3 if (addr>>12)==9 else 2):
        p=addr>>12
        return "vrc6","control" if (p==9 and (addr&3)==3) else VOICES_VRC6[p-9]
    return None

def analyze(text: str) -> dict:
    reader=csv.DictReader(io.StringIO(text))
    if tuple(reader.fieldnames or ())!=FIELDS:
        raise ValueError("Unexpected original 2A03/VRC6 audio trace header")
    last_cycle=-1
    previous={}
    voices=Counter()
    overrides=Counter()
    distinct=Counter()
    count=0
    for rec in reader:
        if set(rec)!=set(FIELDS) or any(v is None or v.strip()!=v or not v for v in rec.values()):
            raise ValueError("Incomplete original sound-register write row")
        try:
            cycle=int(rec["cycle"],10)
            addr=int(rec["register"],16)
            value=int(rec["value"],16)
            old=int(rec["previous"],16)
            delta=int(rec["delta_cycles"],10)
            rapid=int(rec["rapid_rewrite"],10)
        except ValueError as exc:
            raise ValueError("Invalid hex/decimal sound-register record") from exc
        voice=expected(addr)
        if (not voice or tuple(rec[k] for k in ("chip","voice"))!=voice or
            cycle<last_cycle or not 0<=value<=255 or not 0<=old<=255 or
            delta<0 or rapid not in (0,1)):
            raise ValueError("Sound-register address, voice or timestamp mismatch")
        old_info=previous.get(addr)
        expected_delta=cycle-old_info[0] if old_info else 0
        expected_old=old_info[1] if old_info else 0
        expected_rapid=int(old_info is not None and expected_delta<=6000 and
                           expected_old!=value)
        if (delta,old,rapid)!=(expected_delta,expected_old,expected_rapid):
            raise ValueError("Previous value or rapid-write annotation does not match hardware log")
        previous[addr]=(cycle,value)
        last_cycle=cycle
        count+=1
        if count>1000000:
            raise ValueError("Sound-register log exceeds bounded diagnostic size")
        key="/".join(voice)
        voices[key]+=1
        if rapid: overrides[key]+=1
        distinct[key]+=int(old_info is None or expected_old!=value)
    if not count: raise ValueError("No 2A03/VRC6 sound writes captured")
    return {
        "schema":"CVIII-JP-audio-register-traffic/v1",
        "total_writes":count,
        "writes_per_chip_voice":dict(sorted(voices.items())),
        "rapid_value_changes_within_6000_cpu_cycles":dict(sorted(overrides.items())),
        "changed_value_writes":dict(sorted(distinct.items())),
        "rapid_rewrite_events":sum(overrides.values()),
        "source_ownership_proven":False,
        "independent_sfx_polyphony_enabled":False,
        "interpretation":"Hardware register traffic cannot reveal sounds dropped before emission.",
    }

def fixture() -> str:
    data=(
        ("100","2a03","pulse1","4000","21","00","0","0"),
        ("200","2a03","pulse1","4000","81","21","100","1"),
        ("201","2a03","triangle","4008","42","00","0","0"),
        ("300","vrc6","saw","B002","83","00","0","0"),
    )
    stream=io.StringIO()
    w=csv.writer(stream,lineterminator="\n")
    w.writerow(FIELDS)
    w.writerows(data)
    return stream.getvalue()

def smoke() -> None:
    source=fixture()
    result=analyze(source)
    assert result["total_writes"]==4
    assert result["rapid_rewrite_events"]==1
    assert result["writes_per_chip_voice"]["vrc6/saw"]==1
    for broken in (
        source.replace("200,2a03,pulse1", "200,2a03,noise"),
        source.replace("B002", "B003"),
        source.replace("100,1", "101,1"),
        source.replace("200,2a03", "20,2a03"),
        source.replace("81,21", "81,22"),
        source.replace("100,1", "100,0"),
        source.replace("register", "other_field"),
    ):
        try: analyze(broken)
        except ValueError: continue
        raise AssertionError("Corrupt 2A03/VRC6 register trace passed")
    print("PASS JP0610 2A03+VRC6 register ownership boundaries and override timing")

def main() -> None:
    p=argparse.ArgumentParser(description=__doc__)
    g=p.add_mutually_exclusive_group(required=True)
    g.add_argument("--csv",type=Path)
    g.add_argument("--smoke",action="store_true")
    p.add_argument("--json",type=Path)
    a=p.parse_args()
    if a.smoke: smoke(); return
    result=analyze(a.csv.read_text(encoding="utf-8"))
    if a.json: a.json.write_text(json.dumps(result,indent=2)+"\n",encoding="utf-8")
    print(json.dumps(result,indent=2))

if __name__=="__main__":
    main()

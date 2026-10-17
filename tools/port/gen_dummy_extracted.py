#!/usr/bin/env python3
"""Genera datos extraídos FALSOS (todo ceros) a partir de database.json.

Sirve solo para comprobar que el código del port compila y enlaza sin tener
la ROM (por ejemplo en CI). El ejecutable resultante NO es jugable: para un
build real usa `make extract` con tu propia ROM (mzm_us_baserom.gba).
"""
import argparse
import json
from pathlib import Path

ap = argparse.ArgumentParser()
ap.add_argument("-r", "--region", default="us")
ap.add_argument("--root", default=".")
args = ap.parse_args()
root = Path(args.root)

db = json.loads((root / "database.json").read_text())
n = 0
for e in db:
    addr = e["addr"].get(args.region)
    if addr is None:
        continue
    count = e["count"]
    if isinstance(count, dict):
        count = count[args.region]
    count = int(count, 16)
    data_path = Path(e.get("dir", "data"), e["path"])
    inc_path = root / "include" / "extracted" / (str(data_path) + ".inc")
    inc_path.parent.mkdir(parents=True, exist_ok=True)
    if not inc_path.exists():
        inc_path.write_text(",".join(["0u"] * count))
    bin_path = root / data_path
    bin_path.parent.mkdir(parents=True, exist_ok=True)
    if not bin_path.exists():
        bin_path.write_bytes(bytes(count * e["size"]))
    n += 1
print(f"{n} entradas generadas (datos falsos, region {args.region})")

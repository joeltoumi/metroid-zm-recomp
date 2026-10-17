#!/usr/bin/env python3
"""Generate the native build's runtime-asset glue.

The decomp embeds game data at compile time:

    const u8 sTileset_42_Tilemap[1796] = {
        #include "extracted/data/tilesets/42.tm.inc"
    };

The port can't ship that data, so instead (for every src/data/**.c file):

  * a zero-filled stub of each .inc is written to OUT/stub/extracted/..., with
    the element count from database.json, so arrays keep their exact size;
  * a wrapper OUT/wrappers/<file>.c is written that #includes the original
    file and registers, in a constructor, where each array's ROM bytes go:
    (address in memory, ROM offset, byte count). At startup the port copies
    them out of the player's own ROM (port/src/assets.c).

Which includes are live depends on the region #ifdefs, so each data file is
run through the C preprocessor and the expanded output is inspected.

Usage: gen_assets.py --root REPO --out DIR --region us --cc CC [--jobs N]
Prints the list of wrapper files (one per line) on stdout.
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ELEM_SIZES = {
    "u8": 1, "s8": 1, "char": 1, "unsigned char": 1, "signed char": 1,
    "u16": 2, "s16": 2, "short": 2, "unsigned short": 2,
    "u32": 4, "s32": 4, "int": 4, "unsigned int": 4,
}

LINEMARKER = re.compile(r'^# (\d+) "([^"]*)"(.*)$')
# `NAME[..][..] = { prefix,` right before the include (end of the text).
DECL_END = re.compile(r'([A-Za-z_]\w*)\s*((?:\[[^\]]*\]\s*)+)=\s*\{([^{};]*)$', re.S)


def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--region", default="us")
    ap.add_argument("--cc", default="cc")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    return ap.parse_args()


def load_db(root, region):
    db = json.loads((root / "database.json").read_text())
    entries = {}
    for e in db:
        addr = e["addr"].get(region)
        if addr is None:
            continue
        count = e["count"]
        if isinstance(count, dict):
            count = count[region]
        rel = str(Path(e.get("dir", "data"), e["path"]))
        entries[rel] = (int(addr, 16), int(count, 16), e["size"])
    return entries


def write_stubs(out, entries):
    stub_root = out / "stub" / "extracted"
    for rel, (_, count, _) in entries.items():
        p = stub_root / (rel + ".inc")
        text = ",".join(["0"] * count)
        if p.exists() and p.stat().st_size == len(text):
            continue
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text)
    return out / "stub"


def preprocess(cc, root, stub_dir, region, src):
    cmd = [cc, "-E", "-nostdinc", f"-I{stub_dir}", f"-I{root / 'include'}",
           "-DPLATFORM_PC", f"-DREGION_{region.upper()}", str(src)]
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout


def find_sites(pp_text, src, main_names):
    """Yield (symbol, prefix_elems, elem_type, inc_rel) for each extracted include."""
    lines = pp_text.split("\n")
    body = []          # text of the main file so far (since the last site)
    current = None     # file the next lines belong to
    sites = []
    i = 0
    while i < len(lines):
        line = lines[i]
        m = LINEMARKER.match(line)
        if m:
            name = m.group(2)
            if "/extracted/" in name and name.endswith(".inc") and current in main_names:
                rel = name.split("/extracted/", 1)[1][: -len(".inc")]
                text = "\n".join(body).rstrip()
                d = DECL_END.search(text)
                if d is None:
                    raise SystemExit(f"{src}: can't find the array around extracted/{rel}.inc")
                symbol = d.group(1)
                prefix = d.group(3).strip()
                prefix_elems = len([x for x in prefix.split(",") if x.strip()]) if prefix else 0
                type_text = re.split(r"[;}]", text[: d.start()])[-1]
                type_words = re.sub(r"\b(static|const|volatile|extern)\b", " ", type_text).split()
                elem_type = " ".join(type_words)
                sites.append((symbol, prefix_elems, elem_type, rel))
                body = []
            current = name
            i += 1
            continue
        if current in main_names:
            body.append(line)
        i += 1
    return sites


TEXT_MACRO = re.compile(r'INCTEXT\(|SHIFT_JIS\("')


def text_preprocessed(root, out, src):
    """Files with INCTEXT()/SHIFT_JIS() strings go through tools/preproc.py."""
    text = src.read_text(encoding="utf-8", errors="surrogateescape")
    if not TEXT_MACRO.search(text):
        return src
    dst = out / "preproc" / src.relative_to(root)
    result = subprocess.run(
        [sys.executable, str(root / "tools" / "preproc.py"), str(src), str(root / "charmap.txt")],
        check=True, capture_output=True, text=True, encoding="utf-8")
    content = f'#line 1 "{src}"\n' + result.stdout
    dst.parent.mkdir(parents=True, exist_ok=True)
    if not dst.exists() or dst.read_text(encoding="utf-8") != content:
        dst.write_text(content, encoding="utf-8")
    return dst


def gen_wrapper(args, root, out, stub_dir, entries, src):
    real_src = src
    src = text_preprocessed(root, out, src)
    pp = preprocess(args.cc, root, stub_dir, args.region, src)
    sites = find_sites(pp, src, {str(src), str(real_src)})
    rel_src = real_src.relative_to(root)
    wrapper = out / "wrappers" / (str(rel_src).replace("/", "__"))
    lines = [
        "// Generated by tools/port/gen_assets.py — do not edit.",
        f'#include "{src}"',
        "",
    ]
    if sites:
        lines += [
            '#include "port/assets_glue.h"',
            "",
            "static PortAsset sPortAssets[] = {",
        ]
        for symbol, prefix, elem_type, rel in sites:
            if rel not in entries:
                raise SystemExit(f"{src}: {rel} not in database.json for region {args.region}")
            addr, count, size = entries[rel]
            esize = ELEM_SIZES.get(elem_type)
            if esize is None:
                raise SystemExit(f"{src}: unknown element type '{elem_type}' for {symbol}")
            if esize != size:
                raise SystemExit(f"{src}: {symbol} is {elem_type} but {rel} has {size}-byte entries")
            lines.append(f"    {{ (unsigned char*){symbol} + {prefix * esize}, 0x{addr:x}, 0x{count * size:x}, sizeof({symbol}) }}, // {rel}")
        lines += [
            "};",
            "",
            "__attribute__((constructor)) static void PortRegisterAssets(void)",
            "{",
            "    Port_RegisterAssets(sPortAssets, sizeof(sPortAssets) / sizeof(sPortAssets[0]));",
            "}",
        ]
    text = "\n".join(lines) + "\n"
    if not wrapper.exists() or wrapper.read_text() != text:
        wrapper.write_text(text)
    return wrapper, len(sites)


def main():
    args = parse_args()
    root = Path(args.root).resolve()
    out = Path(args.out).resolve()
    (out / "wrappers").mkdir(parents=True, exist_ok=True)

    entries = load_db(root, args.region)
    stub_dir = write_stubs(out, entries)

    data_files = sorted((root / "src" / "data").rglob("*.c"))
    results = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futs = [ex.submit(gen_wrapper, args, root, out, stub_dir, entries, f) for f in data_files]
        for f in futs:
            results.append(f.result())

    total = sum(n for _, n in results)
    sys.stderr.write(f"gen_assets: {len(results)} data files, {total} ROM assets (region {args.region})\n")
    for w, _ in results:
        print(w)


if __name__ == "__main__":
    main()

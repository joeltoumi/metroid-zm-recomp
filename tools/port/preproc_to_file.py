#!/usr/bin/env python3
"""Run tools/preproc.py on SRC with CHARMAP and write the result to OUT.

CMake custom commands can't redirect stdout portably, hence this wrapper.
The output keeps a #line directive so compiler errors point at the original.
"""
import subprocess
import sys
from pathlib import Path

src, charmap, out = sys.argv[1:4]
preproc = Path(__file__).resolve().parent.parent / "preproc.py"
result = subprocess.run(
    [sys.executable, str(preproc), src, charmap],
    check=True, capture_output=True, text=True, encoding="utf-8",
)
Path(out).write_text(f'#line 1 "{src}"\n' + result.stdout, encoding="utf-8")

#!/usr/bin/env python3
"""Run the production AUX wrappers with simulated SHP14B8 DPCD registers."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'WhateverXe/kern_xe.cpp').read_text(encoding='utf-8')
begin = source.index('namespace {\nconstexpr uint32_t PanelLinkRate')
end = source.index('uint32_t XE::wrapTGLI2CAUX(', begin)

with tempfile.TemporaryDirectory(prefix='tgl-optional-panel-rate-') as directory:
    work = Path(directory)
    (work / 'tgl_optional_panel_rate_wrappers.inc').write_text(source[begin:end], encoding='utf-8')
    exe = work / 'check'
    subprocess.run([
        'clang++', '-std=c++17', '-O1', '-g', '-fsanitize=address,undefined',
        '-fno-omit-frame-pointer', '-I', str(work),
        str(root / 'tests/tgl_optional_panel_rate_fixture.cpp'), '-o', str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: SHP14B8 AUX translation, native bypass, reentry and error paths')

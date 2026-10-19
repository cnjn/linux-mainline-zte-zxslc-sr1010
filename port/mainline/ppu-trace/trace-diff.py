#!/usr/bin/env python3
"""Compare ppu-trace outputs: compressed path of each, first divergence, and
register state at divergence. usage: trace-diff.py A.txt [B.txt]"""
import re, sys

def load(path):
    steps, state = [], {}
    for line in open(path):
        m = re.match(r'step=(\d+) pc=(\d+) inst=([0-9a-f]+)(.*)', line)
        if not m:
            continue
        for k, v in re.findall(r' w(\d+)=([0-9a-f]+)', m.group(4)):
            state[int(k)] = int(v, 16)
        steps.append((int(m.group(2)), int(m.group(3), 16), dict(state)))
    return steps

def path(steps):
    out, start, prev = [], None, None
    for pc, _, _ in steps:
        if start is None:
            start = prev = pc
        elif pc == prev + 1:
            prev = pc
        else:
            out.append(f'{start}' if start == prev else f'{start}-{prev}')
            start = prev = pc
    if start is not None:
        out.append(f'{start}' if start == prev else f'{start}-{prev}')
    return ' '.join(out)

def name(k):
    if k < 64: return f'pk{k}'
    if k < 96: return f'r{k-64}'
    if k < 128: return f'rsp{k-96}'
    return f'x{k}'

a = load(sys.argv[1])
print('A:', path(a))
if len(sys.argv) > 2:
    b = load(sys.argv[2])
    print('B:', path(b))
    for i, (sa, sb) in enumerate(zip(a, b)):
        if sa[0] != sb[0]:
            pc, inst, st = a[i-1]
            print(f'diverge after step {i-1} pc={pc} inst={inst:016x}: A->{sa[0]} B->{sb[0]}')
            diff = {k: (st.get(k), b[i-1][2].get(k)) for k in set(st) | set(b[i-1][2])
                    if st.get(k) != b[i-1][2].get(k)}
            for k in sorted(diff):
                x, y = diff[k]
                print(f'  {name(k):6} A={x:08x} B={y:08x}' if x is not None and y is not None else f'  {name(k)} {x} {y}')
            break

import re, sys
sys.path.insert(0, '/home/cnjn/zx279133-docs/port/mainline/ppu-trace')
def load(path):
    steps, state = [], {}
    for line in open(path):
        m = re.match(r'step=(\d+) pc=(\d+) inst=([0-9a-f]+)(.*)', line)
        if not m: continue
        for k, v in re.findall(r' w(\d+)=([0-9a-f]+)', m.group(4)):
            state[int(k)] = int(v, 16)
        steps.append((int(m.group(2)), dict(state)))
    return steps
def name(k):
    return f'pk{k}' if k < 64 else f'r{k-64}' if k < 96 else f'rsp{k-96}' if k < 128 else f'x{k}'
pc = int(sys.argv[1]); a = load(sys.argv[2]); b = load(sys.argv[3])
sa = [s for p, s in a if p == pc][0]; sb = [s for p, s in b if p == pc][0]
for k in sorted(set(sa) | set(sb)):
    if k >= 64 and sa.get(k) != sb.get(k):
        print(f'{name(k):6} A={sa.get(k, 0):08x} B={sb.get(k, 0):08x}')

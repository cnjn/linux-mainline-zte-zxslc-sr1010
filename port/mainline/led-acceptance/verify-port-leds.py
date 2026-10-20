#!/usr/bin/env python3
"""Check carrier, LED trigger state and physical GPIO levels on the RAM image."""
import json
from pathlib import Path
import re
import runpy

ROOT = Path(__file__).resolve().parents[3]
BOARD = runpy.run_path(str(ROOT / 'port/mainline/network-acceptance/nic-rx-accept.py'))['board']
PORTS = {'wan': ('eth0', 26), 'lan1': ('lan1', 2), 'lan2': ('lan2', 13),
         'lan3': ('lan3', 68), 'lan4': ('lan4', 69)}


def main():
    rows = {}
    outputs = []
    for name, (netdev, gpio) in PORTS.items():
        led = f'/sys/class/leds/sr1010:{name}'
        attrs = [f'/sys/class/net/{netdev}/carrier', f'{led}/brightness',
                 f'{led}/link', f'{led}/rx', f'{led}/tx']
        command = f"printf 'LED_STATE {name} %s %s %s %s %s %s\\n' "
        command += f'"$(cat {led}/device_name)" '
        command += ' '.join(f'"$(cat {path})"' for path in attrs)
        base = 0x10d10000 + (gpio // 16) * 0x40
        command += f'; echo GPIO_DIR; devmem {base:#x} 32; '
        command += f'echo GPIO_DATA; devmem {base + 0x14:#x} 32'
        output = BOARD(command)
        outputs.append(output)
        match = re.search(rf'^LED_STATE {name} (\S+) ([01]) (\d+) ([01]) ([01]) ([01])$', output, re.M)
        direction = re.search(r'GPIO_DIR\n(0x[0-9a-f]+)', output, re.I)
        data = re.search(r'GPIO_DATA\n(0x[0-9a-f]+)', output, re.I)
        if not (match and direction and data):
            raise RuntimeError(f'incomplete state for {name}: {output}')
        device, carrier, brightness, link, rx, tx = match.groups()
        level = (int(data[1], 16) >> (gpio % 16)) & 1
        is_output = (int(direction[1], 16) >> (gpio % 16)) & 1
        passed = (device == netdev and link == '1' and rx == tx == '0'
                  and bool(int(brightness)) == bool(int(carrier))
                  and level == 1 - int(carrier) and is_output == 1)
        rows[name] = dict(device=device, carrier=int(carrier), brightness=int(brightness),
                          gpio=gpio, level=level, output=is_output, passed=passed)
    result = {'passed': all(row['passed'] for row in rows.values()),
              'ports': rows, 'output': outputs}
    path = ROOT / 'out/port-led-final-state.json'
    path.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps({'path': str(path), 'passed': result['passed'], 'ports': rows}, indent=2))
    if not result['passed']:
        raise SystemExit(1)


if __name__ == '__main__':
    main()

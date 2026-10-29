"""Compile the owning patch's Radio table with unrelated callbacks stubbed."""
from pathlib import Path
import re
import sys

lines = Path(sys.argv[1]).read_text().splitlines()
postimage = []
in_hunk = False
for line in lines:
    if line.startswith('@@ '):
        in_hunk = True
    elif in_hunk and line.startswith((' ', '+')):
        postimage.append(line[1:])
text = '\n'.join(postimage)
table = re.search(r'const struct Radio_s Radio = \{.*?\n\};', text, re.S).group()
fields = re.findall(r'\.(\w+)\s*=', table)
callbacks = set(re.findall(r'= (SX127x\w+)', table)) - {'SX127xReadRssi'}
output = []
for variant in ('1272', '1276'):
    output.append(f'namespace sx{variant} {{')
    output.append('struct Radio_s {')
    for field in fields:
        output.append('int16_t (*Rssi)(int);' if field == 'Rssi' else f'void (*{field})();')
    if 'Rssi' not in fields:
        output.append('int16_t (*Rssi)(int) = nullptr;')
    output.append('};')
    output.extend(f'static void {callback}() {{}}' for callback in sorted(callbacks))
    mapping = re.search(rf'^#define SX127xReadRssi SX{variant}ReadRssi$', text, re.M)
    if mapping:
        output.append(mapping.group())
    output.append(table)
    output.append('#undef SX127xReadRssi\n}')
Path(sys.argv[2]).write_text('\n'.join(output) + '\n')

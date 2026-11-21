"""Compile production RXPS adapter methods against deterministic radio boundaries."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
parts = []
for name in ('void LoRaRadio::startReceive(', 'void LoRaRadio::enableRxDutyCycle(',
             'void LoRaRadio::applyRxPowerSaving(', 'void LoRaRadio::formatRxPowerSaving('):
    start = source.index(name)
    end = source.index('\n}', start) + 2
    parts.append(source[start:end])
Path(sys.argv[2]).write_text('\n'.join(parts))

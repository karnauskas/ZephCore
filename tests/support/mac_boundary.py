"""Exercise production MAC-only validation with a deterministic HMAC boundary."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
parts = []
for name in ("bool Utils::verifyMAC(", "bool Utils::constantTimeEqual("):
    start = source.index(name)
    end = source.index('\n}', start) + 2
    parts.append(source[start:end])
Path(sys.argv[2]).write_text('\n'.join(parts))

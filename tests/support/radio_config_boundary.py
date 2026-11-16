"""Compile the production adapter's configuration methods against a fake driver."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
start = source.index('static bool configParamsEqual(')
end = source.index('/* ── Lifecycle', start)
Path(sys.argv[2]).write_text(source[start:end])

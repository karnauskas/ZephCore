"""Compile the production companion remote-command allowlist for host tests."""
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text()
start = source.index('static bool isRemoteCompanionCommand(')
end = source.index('\nbool CompanionMesh::handleCommand(', start)
Path(sys.argv[2]).write_text(source[start:end])

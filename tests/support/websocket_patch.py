"""Compile the owning patch's complete poll functions without a west checkout."""
import sys
from pathlib import Path

lines = Path(sys.argv[1]).read_text().splitlines(keepends=True)
postimage = []
in_hunk = False
for line in lines:
	if line.startswith('@@ '):
		in_hunk = True
	elif in_hunk and line.startswith((' ', '+')):
		postimage.append(line[1:])
text = ''.join(postimage)
start = text.index('static inline int websocket_poll_offload(')
end = text.index('#if !defined(CONFIG_NET_TEST)', start)
Path(sys.argv[2]).write_text(text[start:end])

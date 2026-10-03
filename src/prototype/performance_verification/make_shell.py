#!/usr/bin/env python3
"""Record every Make recipe exit while a diagnostic -i run continues after failures."""

import json
import os
import subprocess
import sys

result = subprocess.run(["/bin/bash", *sys.argv[1:]])
record = {"cwd": os.getcwd(), "shell_arguments": sys.argv[1:], "exit": result.returncode}
descriptor = os.open(os.environ["PERFORMANCE_GATE_LOG"], os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
try:
	os.write(descriptor, (json.dumps(record) + "\n").encode())
finally:
	os.close(descriptor)
raise SystemExit(result.returncode if result.returncode >= 0 else 128 - result.returncode)

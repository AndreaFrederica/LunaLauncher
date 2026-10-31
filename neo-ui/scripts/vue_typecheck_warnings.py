#!/usr/bin/env python3
"""Run the strict Vue type checker as a build-blocking validation step.

The filename is retained for compatibility with existing Meson source lists;
the check is now intentionally fail-closed after the warning backlog reached
zero.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--project", default="tsconfig.strict.json")
    args, extra = parser.parse_known_args()
    # Windows exposes pnpm through the command shim; invoking ``pnpm`` directly
    # from CreateProcess raises WinError 2 while the same command works in a
    # terminal. Keep this script usable from both Windows and POSIX builders.
    pnpm = "pnpm.cmd" if sys.platform == "win32" else "pnpm"
    command = [pnpm, "exec", "vue-tsc", "--noEmit", "--project", args.project, *extra]
    result = subprocess.run(command, text=True, capture_output=True)
    output = (result.stdout or "") + (result.stderr or "")
    error_count = 0
    for line in output.splitlines():
        if re.search(r"\berror TS\d+:", line):
            error_count += 1
        print(line)
    if error_count:
        print(f"vue-tsc: {error_count} error(s); blocking build")
    elif result.returncode == 0:
        print("vue-tsc: strict check passed")
    elif not output.strip():
        print(f"vue-tsc: checker exited with status {result.returncode}")
    return result.returncode


if __name__ == "__main__":
    raise SystemExit(main())

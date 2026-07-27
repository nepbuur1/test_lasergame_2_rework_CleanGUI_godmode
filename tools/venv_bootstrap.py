"""Makes the tools run inside tools/venv without you having to activate it.

Every tool calls ensure_venv() as its very first statement, before importing
anything third-party. If the interpreter that is running is not the one in
tools/venv, this re-executes the very same command with that interpreter.
So `python3 tools/img2rgb565.py ...` just works, activated or not.

by Marius Versteegen, 2026
"""

import os
import sys

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
VENV_DIR = os.path.join(TOOLS_DIR, "venv")

# Windows puts the interpreter somewhere else than posix does.
if os.name == "nt":
    VENV_PYTHON = os.path.join(VENV_DIR, "Scripts", "python.exe")
else:
    VENV_PYTHON = os.path.join(VENV_DIR, "bin", "python")

_SETUP_HINT = """
The virtual environment of the tools is missing or incomplete.
Create it once with:

    bash {tools}/setup_venv.sh

After that, run the tools with a plain `python3` again - they find the venv
themselves.
""".strip()


def ensure_venv():
    """Re-executes the current script with tools/venv/bin/python, if needed."""

    # Already running inside the venv? Then there is nothing to do.
    # Compare sys.prefix, not sys.executable: venv/bin/python is a symlink to
    # the system interpreter, so comparing the executables (even with samefile)
    # would call every run "already inside the venv". sys.prefix is what
    # actually shifts to the venv, and it is what site-packages follows.
    if os.path.realpath(sys.prefix) == os.path.realpath(VENV_DIR):
        return

    if not os.path.exists(VENV_PYTHON):
        sys.exit(_SETUP_HINT.format(tools=TOOLS_DIR))

    # execv replaces this process, so nothing below it ever runs.
    # sys.argv[0] is the script, which is exactly what we want to hand over.
    os.execv(VENV_PYTHON, [VENV_PYTHON] + sys.argv)

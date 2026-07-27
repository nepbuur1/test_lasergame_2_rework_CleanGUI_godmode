#!/usr/bin/env bash
# Creates tools/venv and installs requirements.txt into it. Run this once.
# The tools find that venv by themselves afterwards - no activation needed.
#
# by Marius Versteegen, 2026

set -e

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

python3 -m venv "${TOOLS_DIR}/venv"
"${TOOLS_DIR}/venv/bin/python" -m pip install --upgrade pip
"${TOOLS_DIR}/venv/bin/python" -m pip install -r "${TOOLS_DIR}/requirements.txt"

echo
echo "Ready. Run the tools with a plain python3, for example:"
echo "  python3 ${TOOLS_DIR}/allImg2rgb565.py <images folder>"

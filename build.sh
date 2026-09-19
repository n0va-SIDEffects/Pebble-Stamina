#!/usr/bin/env bash
# Aufruf aus Windows:  wsl -d Pebble -u root --cd <Projektordner> bash build.sh [pebble-Argumente]
# Ohne Argumente: pebble build
export PATH="$HOME/.local/bin:$PATH"
export PYTHONWARNINGS=ignore
if [ $# -eq 0 ]; then set -- build; fi
pebble "$@" 2>&1 | grep -v -E 'SyntaxWarning|^\s*"""$'
exit "${PIPESTATUS[0]}"

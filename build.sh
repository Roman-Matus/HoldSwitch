#!/usr/bin/env bash
# Тесты движка (gcc) и сборка HoldSwitch.exe под Windows x64 (zig cc).
# Zig ставится один раз: python3 -m venv .venv && .venv/bin/pip install ziglang
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build dist

gcc -std=c99 -Wall -Wextra -Werror -Isrc tests/test_engine.c src/engine.c -o build/test_engine
./build/test_engine
gcc -std=c99 -Wall -Wextra -Werror -Isrc tests/test_i18n.c src/i18n.c -o build/test_i18n
./build/test_i18n

.venv/bin/python -m ziglang cc -target x86_64-windows-gnu -std=c99 -municode -O2 -s \
    -Wall -Wextra -Werror \
    -Wl,--subsystem,windows \
    src/main.c src/engine.c src/switcher.c src/indicator.c src/settings.c src/caret.c src/i18n.c src/holdswitch.rc \
    -luser32 -lgdi32 -lshell32 -ladvapi32 -lwtsapi32 -loleacc -lole32 -loleaut32 -luuid \
    -o dist/HoldSwitch.exe
rm -f dist/HoldSwitch.pdb
echo "dist/HoldSwitch.exe: $(stat -c %s dist/HoldSwitch.exe) байт"

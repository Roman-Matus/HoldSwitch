#!/usr/bin/env bash
# Сборка и zip для релиза: HoldSwitch.exe, readme.txt, readme_en.txt, LICENSE.txt.
set -euo pipefail
cd "$(dirname "$0")"
./build.sh
VER=$(sed -n 's/^#define HS_VERSION_STR "\(.*\)"/\1/p' src/version.h)
mkdir -p release
ZIP="release/HoldSwitch-$VER-win-x64.zip"
python3 - "$ZIP" <<'PY'
import sys, zipfile
out = sys.argv[1]
lic = open('LICENSE', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n')
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    z.write('dist/HoldSwitch.exe', 'HoldSwitch.exe')
    z.write('dist/readme.txt', 'readme.txt')
    z.write('dist/readme_en.txt', 'readme_en.txt')
    z.writestr('LICENSE.txt', lic)
PY
(cd release && sha256sum "HoldSwitch-$VER-win-x64.zip" > "HoldSwitch-$VER-win-x64.zip.sha256")
echo "$ZIP"; cat "$ZIP.sha256"

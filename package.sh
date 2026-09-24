#!/usr/bin/env bash
# Сборка и zip для релиза: HoldSwitch.exe, readme.txt, readme_en.txt, LICENSE.txt.
set -euo pipefail
cd "$(dirname "$0")"
./build.sh
VER=$(sed -n 's/^#define HS_VERSION_STR "\(.*\)"/\1/p' src/version.h)
mkdir -p release
ZIP="release/HoldSwitch-$VER-win-x64.zip"
EPOCH=${SOURCE_DATE_EPOCH:-$(git log -1 --format=%ct)}
python3 - "$ZIP" "$EPOCH" <<'PY'
import sys, time, zipfile
out, epoch = sys.argv[1], int(sys.argv[2])
stamp = time.gmtime(epoch)[:6]  # одинаковое время у всех файлов — одинаковый zip
lic = open('LICENSE', encoding='utf-8').read().replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8')
files = [
    ('HoldSwitch.exe', open('dist/HoldSwitch.exe', 'rb').read()),
    ('readme.txt', open('dist/readme.txt', 'rb').read()),
    ('readme_en.txt', open('dist/readme_en.txt', 'rb').read()),
    ('LICENSE.txt', lic),
]
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
    for name, data in files:
        info = zipfile.ZipInfo(name, date_time=stamp)
        info.external_attr = 0o644 << 16
        info.compress_type = zipfile.ZIP_DEFLATED
        z.writestr(info, data)
PY
(cd release && sha256sum "HoldSwitch-$VER-win-x64.zip" > "HoldSwitch-$VER-win-x64.zip.sha256")
echo "$ZIP"; cat "$ZIP.sha256"

# HoldSwitch

**Switch the Windows input language by holding a key.**
Hold any letter, digit or symbol key a little longer than usual — about half a
second — and the keyboard layout switches to the next one (EN → RU → EN…).
Held letter keys no longer auto-repeat.

[![Build](https://github.com/Roman-Matus/HoldSwitch/actions/workflows/build.yml/badge.svg)](https://github.com/Roman-Matus/HoldSwitch/actions/workflows/build.yml)

[Русская версия](README.ru.md) · © 2026 .NoxCode · [MIT License](LICENSE)

## Download

Get the latest `HoldSwitch-<version>-win-x64.zip` from [Releases](../../releases),
unpack it and run `HoldSwitch.exe`. No installation is needed. Windows 10/11, x64.

The file is not code-signed yet, so Windows may show *"Windows protected your
PC"*: click **More info → Run anyway**.

### Verify the download

Releases are built by [GitHub Actions](.github/workflows/build.yml) from the
tagged source, and each one carries a signed build provenance attestation.
With the [GitHub CLI](https://cli.github.com/) 2.49 or newer:

```bash
gh attestation verify HoldSwitch-<version>-win-x64.zip --repo Roman-Matus/HoldSwitch
```

The build is reproducible: `./package.sh` on the same commit produces the same
`HoldSwitch.exe` and the same zip, byte for byte.

## Features

- **Three long-press modes**
  - **A** — only switch the language (default);
  - **B** — switch and type the character in the new language (`f` → `а`);
  - **C** — type at once; on hold, erase it and switch.
- **No auto-repeat** for letter, digit and symbol keys. Backspace, arrows and
  shortcuts with Ctrl/Alt/Win keep working as usual.
- **Language indicator** — a small label (`EN`, `RU`…) in the screen corner or
  right above the text cursor.
- Adjustable hold time (300–800 ms), optional space bar, start with Windows.
- Russian and English interface.
- Alt+Shift and Ctrl+Shift keep working.

Settings live in the tray icon menu (**Яz**).

## Privacy

HoldSwitch sees every key press — that is how it works — so here is exactly
what it does with them:

- **Nothing is recorded or sent.** The program has no network code at all
  (it does not link any networking library) and never writes typed text anywhere.
- Settings are stored in `%APPDATA%\HoldSwitch\settings.ini`.
- With the indicator set to *above the text cursor*, it writes
  `%APPDATA%\HoldSwitch\caret.log` for troubleshooting: the method used to find
  the cursor, the program name, the window class and screen coordinates.
  No text. The file is capped at 256 KB.
- To find the text cursor it uses Windows accessibility APIs (UI Automation,
  MSAA), the same ones screen readers use.

The source is short — read it, or build the exe yourself (see below).

## Limitations

- Doesn't work in windows run as administrator (Task Manager, elevated
  terminals) unless HoldSwitch itself runs as administrator.
- Best paused in full-screen games and remote desktop sessions.
- In modes A and B a character appears on key release (or when the next key is
  pressed), because the program must first tell a tap from a hold.
- Characters typed with AltGr and shortcuts with Ctrl/Alt/Win are never
  intercepted.

## Build

The exe is cross-compiled on Linux with [Zig](https://ziglang.org/)'s C
compiler; tests run with the host `gcc`.

```bash
python3 -m venv .venv && .venv/bin/pip install ziglang==0.16.0   # once
./build.sh      # runs tests, builds dist/HoldSwitch.exe
./package.sh    # builds the release zip in release/
```

## How it works

- `src/engine.c` — long-press logic with no Windows API, covered by
  `tests/test_engine.c` (a small simulator of key repeat, timers and layouts).
- `src/main.c` — low-level keyboard hook (`WH_KEYBOARD_LL`), tray icon, menu.
- `src/switcher.c` — layout switch via `WM_INPUTLANGCHANGEREQUEST`, verified,
  with a fallback to the system switch shortcut.
- `src/caret.c` — text cursor lookup: system caret, UI Automation
  (`TextPattern2`, `TextPattern`), MSAA, input field bounds, mouse pointer.
- `src/indicator.c` — the language label; `src/i18n.c` — UI strings
  (covered by `tests/test_i18n.c`); `src/settings.c` — settings and autostart.

Design notes (in Russian): [docs/design.md](docs/design.md).
Manual test checklist: [docs/testing.md](docs/testing.md).

## License

[MIT](LICENSE) © 2026 .NoxCode

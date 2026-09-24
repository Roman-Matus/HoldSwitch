#include <windows.h>
#include "settings.h"

#define SECTION L"HoldSwitch"
#define RUN_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Run"
#define RUN_NAME L"HoldSwitch"

static void ini_path(wchar_t *out)
{
    wchar_t dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) lstrcpyW(dir, L".");
    lstrcatW(dir, L"\\HoldSwitch");
    CreateDirectoryW(dir, NULL);
    wsprintfW(out, L"%s\\settings.ini", dir);
}

int settings_load(hs_settings *s)
{
    wchar_t path[MAX_PATH + 32];
    ini_path(path);
    s->enabled = GetPrivateProfileIntW(SECTION, L"enabled", 1, path) != 0;
    s->mode = (int)GetPrivateProfileIntW(SECTION, L"mode", 0, path);
    if (s->mode < 0 || s->mode > 2) s->mode = 0;
    s->threshold_ms = GetPrivateProfileIntW(SECTION, L"threshold_ms", 400, path);
    if (s->threshold_ms < 150 || s->threshold_ms > 3000) s->threshold_ms = 400;
    s->include_space = GetPrivateProfileIntW(SECTION, L"include_space", 0, path) != 0;
    /* до появления вариантов была только галочка show_indicator */
    int legacy = GetPrivateProfileIntW(SECTION, L"show_indicator", 1, path) != 0;
    s->indicator = (int)GetPrivateProfileIntW(SECTION, L"indicator",
                                               legacy ? HS_INDICATOR_CORNER : HS_INDICATOR_OFF, path);
    if (s->indicator < HS_INDICATOR_OFF || s->indicator > HS_INDICATOR_CARET)
        s->indicator = HS_INDICATOR_CORNER;
    s->ui_lang = (int)GetPrivateProfileIntW(SECTION, L"ui_lang", 0, path);
    if (s->ui_lang < 0 || s->ui_lang > 2) s->ui_lang = 0;
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void put_int(const wchar_t *key, int v, const wchar_t *path)
{
    wchar_t buf[16];
    wsprintfW(buf, L"%d", v);
    WritePrivateProfileStringW(SECTION, key, buf, path);
}

void settings_save(const hs_settings *s)
{
    wchar_t path[MAX_PATH + 32];
    ini_path(path);
    put_int(L"enabled", s->enabled, path);
    put_int(L"mode", s->mode, path);
    put_int(L"threshold_ms", (int)s->threshold_ms, path);
    put_int(L"include_space", s->include_space, path);
    put_int(L"indicator", s->indicator, path);
    put_int(L"ui_lang", s->ui_lang, path);
    WritePrivateProfileStringW(SECTION, L"show_indicator", NULL, path);
}

int autostart_get(void)
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return 0;
    LONG rc = RegQueryValueExW(key, RUN_NAME, NULL, NULL, NULL, NULL);
    RegCloseKey(key);
    return rc == ERROR_SUCCESS;
}

void autostart_set(int on)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RUN_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL,
                        &key, NULL) != ERROR_SUCCESS)
        return;
    if (on) {
        wchar_t exe[MAX_PATH], cmd[MAX_PATH + 4];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wsprintfW(cmd, L"\"%s\"", exe);
        RegSetValueExW(key, RUN_NAME, 0, REG_SZ, (const BYTE *)cmd,
                       (DWORD)((lstrlenW(cmd) + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, RUN_NAME);
    }
    RegCloseKey(key);
}

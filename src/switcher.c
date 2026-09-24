#include <windows.h>
#include "switcher.h"

enum { POLL_MS = 20, POLLS_PER_PHASE = 15 }; /* 300 мс на каждый способ */

static HWND owner;
static UINT_PTR timer_id;
static switch_done_fn on_done;

static struct {
    int active;
    int phase;  /* 0 — сообщение окну, 1 — системное сочетание */
    int polls;
    HWND fg;
    DWORD thread;
    HKL before;
    HKL target;
    int type_after;
    hs_key key;
} job;

void inject_keys(const hs_key *keys, int count)
{
    INPUT in[16];
    int n = 0;
    for (int i = 0; i < count && n < 16; i++, n++) {
        ZeroMemory(&in[n], sizeof in[n]);
        in[n].type = INPUT_KEYBOARD;
        in[n].ki.wVk = keys[i].vk;
        in[n].ki.wScan = keys[i].scan;
        in[n].ki.dwFlags = (keys[i].up ? KEYEVENTF_KEYUP : 0) |
                           (keys[i].extended ? KEYEVENTF_EXTENDEDKEY : 0);
        in[n].ki.dwExtraInfo = HS_INJECT_MARK;
    }
    if (n) SendInput((UINT)n, in, sizeof(INPUT));
}

void switcher_init(HWND o, UINT_PTR id, switch_done_fn done)
{
    owner = o;
    timer_id = id;
    on_done = done;
}

static HKL current_layout(void)
{
    return GetKeyboardLayout(job.thread);
}

static HKL next_layout(HKL cur)
{
    HKL list[32];
    int n = GetKeyboardLayoutList(32, list);
    if (n <= 0) return cur;
    for (int i = 0; i < n; i++)
        if (list[i] == cur) return list[(i + 1) % n];
    return list[0];
}

/* Сочетание, которое пользователь назначил в Windows для смены раскладки. */
static void press_system_hotkey(void)
{
    wchar_t val[8] = L"1";
    DWORD size = sizeof val;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Keyboard Layout\\Toggle", 0, KEY_QUERY_VALUE,
                      &key) == ERROR_SUCCESS) {
        if (RegQueryValueExW(key, L"Hotkey", NULL, NULL, (BYTE *)val, &size) != ERROR_SUCCESS)
            lstrcpyW(val, L"1");
        RegCloseKey(key);
    }
    hs_key seq[4];
    if (val[0] == L'1' || val[0] == L'2') {
        hs_key mod = val[0] == L'1' ? (hs_key){ VK_LMENU, 0x38, 0, 0 }
                                    : (hs_key){ VK_LCONTROL, 0x1D, 0, 0 };
        hs_key shift = { VK_LSHIFT, 0x2A, 0, 0 };
        seq[0] = mod; seq[1] = shift;
        seq[2] = shift; seq[2].up = 1;
        seq[3] = mod; seq[3].up = 1;
    } else { /* сочетание не назначено — Win+Пробел есть всегда (Windows 8+) */
        hs_key win = { VK_LWIN, 0x5B, 1, 0 };
        hs_key space = { VK_SPACE, 0x39, 0, 0 };
        seq[0] = win; seq[1] = space;
        seq[2] = space; seq[2].up = 1;
        seq[3] = win; seq[3].up = 1;
    }
    inject_keys(seq, 4);
}

static void finish(void)
{
    KillTimer(owner, timer_id);
    job.active = 0;
    if (on_done) on_done(current_layout(), job.fg, job.type_after, job.key);
}

void switcher_request(int type_after, hs_key key)
{
    if (job.active) finish();

    job.fg = GetForegroundWindow();
    job.thread = job.fg ? GetWindowThreadProcessId(job.fg, NULL) : 0;
    job.before = current_layout();
    job.target = next_layout(job.before);
    job.type_after = type_after;
    job.key = key;
    job.phase = 0;
    job.polls = 0;
    job.active = 1;

    if (job.target == job.before) { /* одна раскладка — переключать нечего */
        finish();
        return;
    }
    if (job.fg)
        PostMessageW(job.fg, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)job.target);
    else {
        job.phase = 1;
        press_system_hotkey();
    }
    SetTimer(owner, timer_id, POLL_MS, NULL);
}

void switcher_on_timer(void)
{
    if (!job.active) {
        KillTimer(owner, timer_id);
        return;
    }
    if (current_layout() != job.before) {
        finish();
        return;
    }
    if (++job.polls < POLLS_PER_PHASE) return;
    if (job.phase == 0) {
        job.phase = 1;
        job.polls = 0;
        press_system_hotkey();
        return;
    }
    finish();
}

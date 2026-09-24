#include <windows.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include "caret.h"
#include "engine.h"
#include "i18n.h"
#include "indicator.h"
#include "settings.h"
#include "switcher.h"
#include "version.h"

#define APP_NAME L"HoldSwitch"
#define WM_TRAY (WM_APP + 1)
#define WM_CARET_FOUND (WM_APP + 2)

enum { TIMER_HOLD = 1, TIMER_SWITCH = 2 };
enum { TRAY_ID = 1 };

enum {
    CMD_ENABLED = 100,
    CMD_MODE_BASE = 110,       /* + hs_mode */
    CMD_THRESHOLD_BASE = 120,  /* + индекс в thresholds */
    CMD_INDICATOR_BASE = 130,  /* + hs_indicator */
    CMD_SPACE = 140,
    CMD_AUTOSTART = 142,
    CMD_EXIT = 150,
    CMD_ABOUT = 151,
    CMD_UI_BASE = 160          /* + hs_ui_lang */
};

static const unsigned thresholds[] = { 300, 400, 500, 600, 800 };
#define THRESHOLD_COUNT ((int)(sizeof thresholds / sizeof thresholds[0]))

static HINSTANCE inst;
static HWND main_wnd;
static HHOOK hook;
static hs_engine engine;
static hs_settings cfg;
static HICON icon_on, icon_off;
static UINT taskbar_created_msg;
static HWINEVENTHOOK foreground_hook;

/* Плашка у курсора ввода: ждём ответа от потока поиска курсора. */
static WPARAM caret_seq;
static HKL caret_layout;

/* ---------- действия движка ---------- */

static void apply(hs_result r)
{
    hs_key batch[HS_MAX_ACTIONS];
    int n = 0;
    for (int i = 0; i < r.count; i++) {
        hs_action a = r.actions[i];
        if (a.kind == HS_ACT_INJECT) {
            batch[n++] = a.key;
            continue;
        }
        inject_keys(batch, n);
        n = 0;
        switch (a.kind) {
        case HS_ACT_TIMER_START: SetTimer(main_wnd, TIMER_HOLD, a.ms, NULL); break;
        case HS_ACT_TIMER_STOP: KillTimer(main_wnd, TIMER_HOLD); break;
        case HS_ACT_SWITCH: switcher_request(a.type_after, a.key); break;
        default: break;
        }
    }
    inject_keys(batch, n);
}

static void on_switch_done(HKL actual, HWND fg, int type_after, hs_key key)
{
    if (type_after) {
        hs_key tap[2] = { key, key };
        tap[0].up = 0;
        tap[1].up = 1;
        inject_keys(tap, 2);
    }
    if (cfg.indicator == HS_INDICATOR_CORNER) {
        indicator_show(actual, fg);
    } else if (cfg.indicator == HS_INDICATOR_CARET) {
        caret_layout = actual;
        caret_locate_async(main_wnd, WM_CARET_FOUND, ++caret_seq, fg);
    }
}

static LRESULT CALLBACK keyboard_hook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        const KBDLLHOOKSTRUCT *p = (const KBDLLHOOKSTRUCT *)lp;
        hs_key k;
        k.vk = (unsigned short)p->vkCode;
        k.scan = (unsigned short)p->scanCode;
        k.extended = (p->flags & LLKHF_EXTENDED) ? 1 : 0;
        k.up = (p->flags & LLKHF_UP) ? 1 : 0;
        int injected = (p->flags & LLKHF_INJECTED) != 0;
        hs_result r = hs_on_key(&engine, k, injected);
        apply(r);
        if (r.suppress) return 1;
    }
    return CallNextHookEx(hook, code, wp, lp);
}

/* Смена активного окна: заранее будим его специальные возможности,
 * чтобы плашка у курсора ввода не промахнулась при первом переключении. */
static void CALLBACK on_foreground(HWINEVENTHOOK h, DWORD event, HWND wnd, LONG obj, LONG child,
                                   DWORD thread, DWORD time)
{
    (void)h; (void)event; (void)obj; (void)child; (void)thread; (void)time;
    if (cfg.indicator == HS_INDICATOR_CARET) caret_warm_up(wnd);
}

/* ---------- настройки ---------- */

static void push_settings_to_engine(void)
{
    engine.enabled = cfg.enabled;
    engine.mode = (hs_mode)cfg.mode;
    engine.threshold_ms = cfg.threshold_ms;
    engine.include_space = cfg.include_space;
}

static void apply_ui_lang(void)
{
    i18n_use(i18n_resolve(cfg.ui_lang, PRIMARYLANGID(GetUserDefaultUILanguage())));
}

/* ---------- трей ---------- */

static HICON make_icon(COLORREF fill)
{
    int s = GetSystemMetrics(SM_CXSMICON);
    HDC screen = GetDC(NULL);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP color = CreateCompatibleBitmap(screen, s, s);
    HBITMAP mask = CreateBitmap(s, s, 1, 1, NULL);
    ReleaseDC(NULL, screen);

    RECT all = { 0, 0, s, s };
    int round = s / 3;

    /* маска: 1 — прозрачно, 0 — рисуем */
    HGDIOBJ old = SelectObject(dc, mask);
    FillRect(dc, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SelectObject(dc, GetStockObject(BLACK_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, 0, 0, s + 1, s + 1, round, round);

    SelectObject(dc, color);
    FillRect(dc, &all, (HBRUSH)GetStockObject(BLACK_BRUSH));
    HBRUSH brush = CreateSolidBrush(fill);
    SelectObject(dc, brush);
    RoundRect(dc, 0, 0, s + 1, s + 1, round, round);

    HFONT font = CreateFontW(-(s * 3 / 4), 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, L"Яz", -1, &all, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(dc, old);
    DeleteDC(dc); /* сначала DC: выбранные в него кисть и шрифт не удаляются */
    DeleteObject(font);
    DeleteObject(brush);

    ICONINFO ii = { TRUE, 0, 0, mask, color };
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

static const wchar_t *mode_title(int mode)
{
    switch (mode) {
    case HS_MODE_SWITCH_AND_TYPE: return tr(S_TIP_MODE_B);
    case HS_MODE_TYPE_THEN_ERASE: return tr(S_TIP_MODE_C);
    default: return tr(S_TIP_MODE_A);
    }
}

static void fill_tray(NOTIFYICONDATAW *nid)
{
    ZeroMemory(nid, sizeof *nid);
    nid->cbSize = sizeof *nid;
    nid->hWnd = main_wnd;
    nid->uID = TRAY_ID;
    nid->uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid->uCallbackMessage = WM_TRAY;
    nid->hIcon = cfg.enabled ? icon_on : icon_off;
    if (cfg.enabled)
        wsprintfW(nid->szTip, tr(S_TIP_FORMAT), mode_title(cfg.mode), cfg.threshold_ms);
    else
        lstrcpyW(nid->szTip, tr(S_TIP_OFF));
}

static void tray_add(void)
{
    NOTIFYICONDATAW nid;
    fill_tray(&nid);
    Shell_NotifyIconW(NIM_ADD, &nid);
}

static void tray_update(void)
{
    NOTIFYICONDATAW nid;
    fill_tray(&nid);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void tray_balloon(const wchar_t *title, const wchar_t *text)
{
    NOTIFYICONDATAW nid;
    fill_tray(&nid);
    nid.uFlags = NIF_INFO;
    lstrcpynW(nid.szInfoTitle, title, sizeof nid.szInfoTitle / sizeof nid.szInfoTitle[0]);
    lstrcpynW(nid.szInfo, text, sizeof nid.szInfo / sizeof nid.szInfo[0]);
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

static void tray_remove(void)
{
    NOTIFYICONDATAW nid;
    fill_tray(&nid);
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

static void show_menu(void)
{
    HMENU menu = CreatePopupMenu();
    HMENU modes = CreatePopupMenu();
    HMENU delays = CreatePopupMenu();
    HMENU indicators = CreatePopupMenu();
    HMENU ui_langs = CreatePopupMenu();

    AppendMenuW(ui_langs, MF_STRING, CMD_UI_BASE + HS_UI_AUTO, tr(S_UI_AUTO));
    AppendMenuW(ui_langs, MF_STRING, CMD_UI_BASE + HS_UI_RU, tr(S_UI_RU));
    AppendMenuW(ui_langs, MF_STRING, CMD_UI_BASE + HS_UI_EN, tr(S_UI_EN));
    CheckMenuRadioItem(ui_langs, CMD_UI_BASE, CMD_UI_BASE + 2, CMD_UI_BASE + cfg.ui_lang,
                       MF_BYCOMMAND);

    AppendMenuW(modes, MF_STRING, CMD_MODE_BASE + HS_MODE_SWITCH_ONLY,
                tr(S_MENU_MODE_A));
    AppendMenuW(modes, MF_STRING, CMD_MODE_BASE + HS_MODE_SWITCH_AND_TYPE,
                tr(S_MENU_MODE_B));
    AppendMenuW(modes, MF_STRING, CMD_MODE_BASE + HS_MODE_TYPE_THEN_ERASE,
                tr(S_MENU_MODE_C));
    CheckMenuRadioItem(modes, CMD_MODE_BASE, CMD_MODE_BASE + 2, CMD_MODE_BASE + cfg.mode,
                       MF_BYCOMMAND);

    int checked = -1;
    for (int i = 0; i < THRESHOLD_COUNT; i++) {
        wchar_t text[32];
        wsprintfW(text, tr(S_MS_FORMAT), thresholds[i]);
        AppendMenuW(delays, MF_STRING, CMD_THRESHOLD_BASE + i, text);
        if (thresholds[i] == cfg.threshold_ms) checked = i;
    }
    if (checked >= 0)
        CheckMenuRadioItem(delays, CMD_THRESHOLD_BASE, CMD_THRESHOLD_BASE + THRESHOLD_COUNT - 1,
                           CMD_THRESHOLD_BASE + checked, MF_BYCOMMAND);

    AppendMenuW(indicators, MF_STRING, CMD_INDICATOR_BASE + HS_INDICATOR_CORNER,
                tr(S_IND_CORNER));
    AppendMenuW(indicators, MF_STRING, CMD_INDICATOR_BASE + HS_INDICATOR_CARET,
                tr(S_IND_CARET));
    AppendMenuW(indicators, MF_STRING, CMD_INDICATOR_BASE + HS_INDICATOR_OFF,
                tr(S_IND_OFF));
    CheckMenuRadioItem(indicators, CMD_INDICATOR_BASE, CMD_INDICATOR_BASE + 2,
                       CMD_INDICATOR_BASE + cfg.indicator, MF_BYCOMMAND);

    AppendMenuW(menu, MF_STRING | (cfg.enabled ? MF_CHECKED : 0), CMD_ENABLED, tr(S_ENABLED));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)modes, tr(S_ON_HOLD));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)delays, tr(S_DURATION));
    AppendMenuW(menu, MF_STRING | (cfg.include_space ? MF_CHECKED : 0), CMD_SPACE,
                tr(S_SPACE));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)indicators, tr(S_INDICATOR));
    AppendMenuW(menu, MF_STRING | (autostart_get() ? MF_CHECKED : 0), CMD_AUTOSTART,
                tr(S_AUTOSTART));
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)ui_langs, tr(S_UI_LANG));
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, CMD_ABOUT, tr(S_ABOUT));
    AppendMenuW(menu, MF_STRING, CMD_EXIT, tr(S_EXIT));

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(main_wnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, main_wnd, NULL);
    PostMessageW(main_wnd, WM_NULL, 0, 0);
    DestroyMenu(menu); /* подменю уничтожаются вместе с ним */
}

static void on_command(int id)
{
    if (id == CMD_EXIT) {
        DestroyWindow(main_wnd);
        return;
    }
    if (id == CMD_ABOUT) {
        wchar_t text[512];
        wsprintfW(text, tr(S_ABOUT_TEXT), L"" HS_VERSION_STR, L"" HS_YEAR, L"" HS_AUTHOR);
        MessageBoxW(main_wnd, text, APP_NAME, MB_ICONINFORMATION);
        return;
    }
    if (id == CMD_AUTOSTART) {
        autostart_set(!autostart_get());
        return;
    }

    apply(hs_flush(&engine)); /* незавершённое удержание — как обычное нажатие */
    if (id == CMD_ENABLED) cfg.enabled = !cfg.enabled;
    else if (id == CMD_SPACE) cfg.include_space = !cfg.include_space;
    else if (id >= CMD_INDICATOR_BASE && id <= CMD_INDICATOR_BASE + 2)
        cfg.indicator = id - CMD_INDICATOR_BASE;
    else if (id >= CMD_UI_BASE && id <= CMD_UI_BASE + 2) {
        cfg.ui_lang = id - CMD_UI_BASE;
        apply_ui_lang();
    }
    else if (id >= CMD_MODE_BASE && id <= CMD_MODE_BASE + 2) cfg.mode = id - CMD_MODE_BASE;
    else if (id >= CMD_THRESHOLD_BASE && id < CMD_THRESHOLD_BASE + THRESHOLD_COUNT)
        cfg.threshold_ms = thresholds[id - CMD_THRESHOLD_BASE];
    push_settings_to_engine();
    settings_save(&cfg);
    tray_update();
}

/* Координаты курсора ввода, указателя и мониторов — в физических пикселях
 * (Windows 10 1703+); на старых системах — системный масштаб. */
static void enable_dpi_awareness(void)
{
    typedef BOOL(WINAPI * set_ctx_fn)(DPI_AWARENESS_CONTEXT);
    set_ctx_fn set_ctx = (set_ctx_fn)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                            "SetProcessDpiAwarenessContext");
    if (!set_ctx || !set_ctx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
        enable_dpi_awareness();
}

/* ---------- окно ---------- */

static LRESULT CALLBACK main_proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        if (wp == TIMER_HOLD) {
            KillTimer(h, TIMER_HOLD);
            apply(hs_on_timer(&engine));
        } else if (wp == TIMER_SWITCH) {
            switcher_on_timer();
        }
        return 0;
    case WM_TRAY:
        if (lp == WM_RBUTTONUP || lp == WM_LBUTTONUP) show_menu();
        return 0;
    case WM_CARET_FOUND: {
        RECT *rc = (RECT *)lp;
        if (wp == caret_seq && cfg.indicator == HS_INDICATOR_CARET)
            indicator_show_near(caret_layout, rc);
        HeapFree(GetProcessHeap(), 0, rc);
        return 0;
    }
    case WM_COMMAND:
        on_command(LOWORD(wp));
        return 0;
    case WM_WTSSESSION_CHANGE:
        /* пока открыт экран блокировки, хук не видит отпусканий */
        KillTimer(h, TIMER_HOLD);
        hs_reset(&engine);
        return 0;
    case WM_DESTROY:
        tray_remove();
        PostQuitMessage(0);
        return 0;
    }
    if (msg == taskbar_created_msg && taskbar_created_msg) { /* проводник перезапустился */
        tray_add();
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, PWSTR cmd, int show)
{
    (void)prev; (void)cmd; (void)show;
    inst = instance;

    int had_settings = settings_load(&cfg);
    apply_ui_lang();

    HANDLE mutex = CreateMutexW(NULL, TRUE, L"Local\\HoldSwitch.SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxW(NULL, tr(S_ALREADY_RUNNING), APP_NAME, MB_ICONINFORMATION);
        return 0;
    }

    enable_dpi_awareness();

    hs_init(&engine);
    push_settings_to_engine();

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = main_proc;
    wc.hInstance = inst;
    wc.lpszClassName = L"HoldSwitchMain";
    RegisterClassW(&wc);
    main_wnd = CreateWindowExW(WS_EX_TOOLWINDOW, wc.lpszClassName, APP_NAME, WS_POPUP,
                               0, 0, 0, 0, NULL, NULL, inst, NULL);

    taskbar_created_msg = RegisterWindowMessageW(L"TaskbarCreated");
    indicator_init(inst);
    switcher_init(main_wnd, TIMER_SWITCH, on_switch_done);

    icon_on = make_icon(HS_ACCENT);
    icon_off = make_icon(RGB(128, 128, 128));
    tray_add();

    hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook, inst, 0);
    if (!hook) {
        MessageBoxW(NULL, tr(S_HOOK_FAILED), APP_NAME, MB_ICONERROR);
        tray_remove();
        return 1;
    }
    WTSRegisterSessionNotification(main_wnd, NOTIFY_FOR_THIS_SESSION);
    foreground_hook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, NULL,
                                      on_foreground, 0, 0,
                                      WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (cfg.indicator == HS_INDICATOR_CARET) caret_warm_up(GetForegroundWindow());

    if (!had_settings) {
        settings_save(&cfg);
        tray_balloon(tr(S_BALLOON_TITLE), tr(S_BALLOON_TEXT));
    }

    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    if (foreground_hook) UnhookWinEvent(foreground_hook);
    WTSUnRegisterSessionNotification(main_wnd);
    UnhookWindowsHookEx(hook);
    DestroyIcon(icon_on);
    DestroyIcon(icon_off);
    CloseHandle(mutex);
    return 0;
}

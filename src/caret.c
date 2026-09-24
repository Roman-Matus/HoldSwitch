#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <oleacc.h>
#include <uiautomationclient.h>
#include "caret.h"

typedef struct {
    HWND notify;
    UINT msg;
    WPARAM seq;
    HWND fg;
} request;

/* ---------- журнал диагностики: какой способ сработал и где ---------- */

enum { LOG_LIMIT = 256 * 1024 };

static void log_line(const wchar_t *method, HWND wnd, const RECT *rc, const wchar_t *detail)
{
    wchar_t path[MAX_PATH + 32], dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", dir, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return;
    wsprintfW(path, L"%s\\HoldSwitch\\caret.log", dir);

    wchar_t cls[64] = L"?", exe[MAX_PATH] = L"?";
    if (wnd) GetClassNameW(wnd, cls, 64);
    DWORD pid = 0;
    if (wnd) GetWindowThreadProcessId(wnd, &pid);
    HANDLE proc = pid ? OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid) : NULL;
    if (proc) {
        DWORD len = MAX_PATH;
        if (QueryFullProcessImageNameW(proc, 0, exe, &len)) {
            wchar_t *slash = exe;
            for (wchar_t *p = exe; *p; p++)
                if (*p == L'\\') slash = p + 1;
            if (slash != exe) lstrcpyW(exe, slash);
        }
        CloseHandle(proc);
    }

    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t line[1024];
    wsprintfW(line, L"%04d-%02d-%02d %02d:%02d:%02d  %-6s  %s  %s  [%ld,%ld %ldx%ld] |%s\r\n",
              t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, method, exe, cls,
              rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top, detail);
    char utf8[2048];
    int bytes = WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof utf8, NULL, NULL) - 1;
    if (bytes <= 0) return;

    HANDLE f = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    if (GetFileSize(f, NULL) > LOG_LIMIT) { /* журнал не растёт бесконечно */
        CloseHandle(f);
        f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (f == INVALID_HANDLE_VALUE) return;
    }
    DWORD written;
    WriteFile(f, utf8, (DWORD)bytes, &written, NULL);
    CloseHandle(f);
}

/* ---------- 1. системный курсор ввода: классические окна ---------- */

static int from_gui_thread(DWORD thread, RECT *out, HWND *focus)
{
    GUITHREADINFO gti;
    ZeroMemory(&gti, sizeof gti);
    gti.cbSize = sizeof gti;
    if (!GetGUIThreadInfo(thread, &gti)) return 0;
    *focus = gti.hwndFocus;
    if (!gti.hwndCaret || gti.rcCaret.bottom <= gti.rcCaret.top) return 0;
    *out = gti.rcCaret;
    MapWindowPoints(gti.hwndCaret, NULL, (POINT *)out, 2);
    return 1;
}

/* ---------- 2. UI Automation: Windows 11, Word, Chrome, Edge ---------- */

/* Прямоугольник диапазона текста: первый (или последний) из GetBoundingRectangles. */
static int range_rect(IUIAutomationTextRange *r, RECT *out, int take_last)
{
    SAFEARRAY *arr = NULL;
    if (FAILED(IUIAutomationTextRange_GetBoundingRectangles(r, &arr)) || !arr) return 0;
    int ok = 0;
    LONG lo = 0, hi = -1;
    SafeArrayGetLBound(arr, 1, &lo);
    SafeArrayGetUBound(arr, 1, &hi);
    LONG count = (hi - lo + 1) / 4; /* left, top, width, height */
    double *d = NULL;
    if (count > 0 && SUCCEEDED(SafeArrayAccessData(arr, (void **)&d))) {
        LONG i = take_last ? count - 1 : 0;
        double left = d[i * 4], top = d[i * 4 + 1], w = d[i * 4 + 2], h = d[i * 4 + 3];
        if (h > 0) {
            SetRect(out, (int)left, (int)top, (int)(left + (w > 0 ? w : 1)), (int)(top + h));
            ok = 1;
        }
        SafeArrayUnaccessData(arr);
    }
    SafeArrayDestroy(arr);
    return ok;
}

/* Курсор — пустой диапазон, у которого часто нет своего прямоугольника.
 * Тогда берём соседний символ: левый край следующего или правый край предыдущего. */
static int caret_from_range(IUIAutomationTextRange *r, RECT *out)
{
    if (range_rect(r, out, 0)) {
        out->right = out->left + 1;
        return 1;
    }
    IUIAutomationTextRange *c = NULL;
    int moved = 0, ok = 0;
    if (SUCCEEDED(IUIAutomationTextRange_Clone(r, &c)) && c) {
        if (SUCCEEDED(IUIAutomationTextRange_MoveEndpointByUnit(
                c, TextPatternRangeEndpoint_End, TextUnit_Character, 1, &moved)) &&
            moved == 1 && range_rect(c, out, 0)) {
            out->right = out->left + 1;
            ok = 1;
        }
        IUIAutomationTextRange_Release(c);
    }
    if (ok) return 1;
    c = NULL;
    if (SUCCEEDED(IUIAutomationTextRange_Clone(r, &c)) && c) {
        if (SUCCEEDED(IUIAutomationTextRange_MoveEndpointByUnit(
                c, TextPatternRangeEndpoint_Start, TextUnit_Character, -1, &moved)) &&
            moved == -1 && range_rect(c, out, 1)) {
            out->left = out->right - 1;
            ok = 1;
        }
        IUIAutomationTextRange_Release(c);
    }
    return ok;
}

/* Что сообщила UI Automation: курсор (из TextPattern2 или выделения) и рамка поля. */
typedef struct {
    int caret_ok;
    const wchar_t *caret_src; /* L"tp2" | L"sel" */
    RECT caret;
    int field_ok;
    RECT field;
} uia_info;

static void from_uia(uia_info *u)
{
    IUIAutomation *uia = NULL;
    if (FAILED(CoCreateInstance(&CLSID_CUIAutomation, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IUIAutomation, (void **)&uia)) || !uia)
        return;

    IUIAutomationElement *el = NULL;
    if (SUCCEEDED(IUIAutomation_GetFocusedElement(uia, &el)) && el) {
        IUIAutomationTextPattern2 *tp2 = NULL;
        if (SUCCEEDED(IUIAutomationElement_GetCurrentPatternAs(
                el, UIA_TextPattern2Id, &IID_IUIAutomationTextPattern2, (void **)&tp2)) && tp2) {
            BOOL active = FALSE;
            IUIAutomationTextRange *r = NULL;
            if (SUCCEEDED(IUIAutomationTextPattern2_GetCaretRange(tp2, &active, &r)) && r) {
                if (caret_from_range(r, &u->caret)) {
                    u->caret_ok = 1;
                    u->caret_src = L"tp2";
                }
                IUIAutomationTextRange_Release(r);
            }
            IUIAutomationTextPattern2_Release(tp2);
        }

        IUIAutomationTextPattern *tp = NULL;
        if (!u->caret_ok && SUCCEEDED(IUIAutomationElement_GetCurrentPatternAs(
                                el, UIA_TextPatternId, &IID_IUIAutomationTextPattern, (void **)&tp)) &&
            tp) {
            IUIAutomationTextRangeArray *sel = NULL;
            int len = 0;
            if (SUCCEEDED(IUIAutomationTextPattern_GetSelection(tp, &sel)) && sel) {
                IUIAutomationTextRange *r = NULL;
                if (SUCCEEDED(IUIAutomationTextRangeArray_get_Length(sel, &len)) && len > 0 &&
                    SUCCEEDED(IUIAutomationTextRangeArray_GetElement(sel, 0, &r)) && r) {
                    if (caret_from_range(r, &u->caret)) {
                        u->caret_ok = 1;
                        u->caret_src = L"sel";
                    }
                    IUIAutomationTextRange_Release(r);
                }
                IUIAutomationTextRangeArray_Release(sel);
            }
            IUIAutomationTextPattern_Release(tp);
        }

        RECT fr;
        if (SUCCEEDED(IUIAutomationElement_get_CurrentBoundingRectangle(el, &fr)) &&
            fr.right > fr.left && fr.bottom > fr.top) {
            u->field = fr;
            u->field_ok = 1;
        }
        IUIAutomationElement_Release(el);
    }
    IUIAutomation_Release(uia);
}

/* ---------- 3. MSAA: запасной путь для Chromium и Electron ---------- */

static int from_accessibility(HWND wnd, RECT *out)
{
    IAccessible *acc = NULL;
    if (FAILED(AccessibleObjectFromWindow(wnd, (DWORD)OBJID_CARET, &IID_IAccessible,
                                          (void **)&acc)) || !acc)
        return 0;
    VARIANT self;
    VariantInit(&self);
    self.vt = VT_I4;
    self.lVal = CHILDID_SELF;
    long x = 0, y = 0, w = 0, h = 0;
    HRESULT hr = IAccessible_accLocation(acc, &x, &y, &w, &h, self);
    IAccessible_Release(acc);
    if (FAILED(hr) || h <= 0 || (x == 0 && y == 0)) return 0;
    SetRect(out, x, y, x + (w > 0 ? w : 1), y + h);
    return 1;
}

/* ---------- поток поиска ---------- */

enum { UIA_ATTEMPTS = 3, UIA_RETRY_MS = 150 };

/* Курсор правдоподобен, если лежит внутри рамки (с допуском в полстроки).
 * Некоторые программы (Telegram) отдают устаревшие координаты курсора:
 * поле ввода выросло, а курсор посчитан от его прежнего положения. */
static int inside(const RECT *caret, const RECT *box)
{
    int tol = (caret->bottom - caret->top) / 2;
    if (tol < 4) tol = 4;
    return caret->top >= box->top - tol && caret->bottom <= box->bottom + tol &&
           caret->left >= box->left - tol && caret->left <= box->right + tol;
}

static void put_rect(wchar_t *buf, const wchar_t *name, int ok, const RECT *r)
{
    wchar_t part[80];
    if (ok)
        wsprintfW(part, L" %s=%ld,%ld,%ldx%ld", name, r->left, r->top, r->right - r->left,
                  r->bottom - r->top);
    else
        wsprintfW(part, L" %s=-", name);
    lstrcatW(buf, part);
}

/* Системный курсор ненадёжен, если это служебная точка, а не черта курсора
 * (Qt ставит 2×2 для методов ввода и обновляет её только при получении
 * фокуса — в Telegram она отстаёт на строку), или окно — Qt. */
static int gui_caret_weak(const RECT *caret, HWND wnd)
{
    if (caret->bottom - caret->top < 6) return 1;
    wchar_t cls[64] = L"";
    if (wnd) GetClassNameW(wnd, cls, 64);
    return cls[0] == L'Q' && cls[1] == L't';
}

static DWORD WINAPI worker(LPVOID param)
{
    request req = *(request *)param;
    HeapFree(GetProcessHeap(), 0, param);

    RECT *rc = (RECT *)HeapAlloc(GetProcessHeap(), 0, sizeof(RECT));
    if (!rc) return 0;

    HWND focus = NULL;
    RECT gui = { 0 }, msaa = { 0 }, win = { 0 };
    uia_info u;
    ZeroMemory(&u, sizeof u);
    DWORD thread = req.fg ? GetWindowThreadProcessId(req.fg, NULL) : 0;
    int gui_ok = thread && from_gui_thread(thread, &gui, &focus);
    int win_ok = req.fg && GetWindowRect(req.fg, &win);
    int gui_weak = gui_ok && gui_caret_weak(&gui, focus ? focus : req.fg);
    int msaa_ok = 0, uia_plausible = 0, attempts = 0;
    /* при запасном системном курсоре хватит двух попыток */
    int max_attempts = gui_ok ? 2 : UIA_ATTEMPTS;
    if ((!gui_ok || gui_weak) && req.fg) {
        CoInitializeEx(NULL, COINIT_MULTITHREADED);
        /* Qt и Chromium включают поддержку специальных возможностей по первому
         * запросу и первый ответ дают пустым — спрашиваем ещё раз. */
        for (attempts = 1; attempts <= UIA_ATTEMPTS; attempts++) {
            ZeroMemory(&u, sizeof u);
            from_uia(&u);
            uia_plausible = u.caret_ok && (!u.field_ok || inside(&u.caret, &u.field)) &&
                            (!win_ok || inside(&u.caret, &win));
            if (uia_plausible || attempts == max_attempts) break;
            Sleep(UIA_RETRY_MS);
        }
        msaa_ok = from_accessibility(focus ? focus : req.fg, &msaa);
        CoUninitialize();
    }

    const wchar_t *method = L"mouse";
    int msaa_plausible = msaa_ok && (!win_ok || inside(&msaa, &win));
    int field_usable = u.field_ok && u.field.bottom - u.field.top <= 400;
    if (gui_ok && !gui_weak) {
        *rc = gui;
        method = L"caret";
    } else if (uia_plausible) {
        *rc = u.caret;
        method = L"uia";
    } else if (gui_ok) {
        *rc = gui;
        method = L"caret?"; /* ненадёжный, но лучше рамки поля */
    } else if (msaa_plausible) {
        *rc = msaa;
        method = L"msaa";
    } else if (field_usable) {
        SetRect(rc, u.field.left, u.field.top, u.field.left + 1, u.field.bottom);
        method = L"field";
    } else {
        POINT pt;
        GetCursorPos(&pt);
        SetRect(rc, pt.x, pt.y, pt.x + 1, pt.y + 1);
    }

    wchar_t detail[400];
    wsprintfW(detail, L" try=%d", attempts);
    put_rect(detail, L"gui", gui_ok, &gui);
    put_rect(detail, u.caret_src ? u.caret_src : L"uia", u.caret_ok, &u.caret);
    put_rect(detail, L"field", u.field_ok, &u.field);
    put_rect(detail, L"msaa", msaa_ok, &msaa);
    put_rect(detail, L"win", win_ok, &win);
    log_line(method, focus ? focus : req.fg, rc, detail);

    if (!PostMessageW(req.notify, req.msg, req.seq, (LPARAM)rc))
        HeapFree(GetProcessHeap(), 0, rc);
    return 0;
}

/* ---------- разогрев: разбудить специальные возможности окна заранее ---------- */

static volatile LONG warming;

static DWORD WINAPI warm_worker(LPVOID param)
{
    HWND fg = (HWND)param;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    uia_info u;
    ZeroMemory(&u, sizeof u);
    from_uia(&u);
    RECT unused;
    from_accessibility(fg, &unused);
    CoUninitialize();
    InterlockedExchange(&warming, 0);
    return 0;
}

void caret_warm_up(HWND fg)
{
    if (!fg || InterlockedCompareExchange(&warming, 1, 0) != 0) return; /* уже греется */
    HANDLE t = CreateThread(NULL, 0, warm_worker, fg, 0, NULL);
    if (t) CloseHandle(t);
    else InterlockedExchange(&warming, 0);
}

void caret_locate_async(HWND notify, UINT msg, WPARAM seq, HWND fg)
{
    request *req = (request *)HeapAlloc(GetProcessHeap(), 0, sizeof *req);
    if (!req) return;
    req->notify = notify;
    req->msg = msg;
    req->seq = seq;
    req->fg = fg;
    HANDLE t = CreateThread(NULL, 0, worker, req, 0, NULL);
    if (t) CloseHandle(t);
    else HeapFree(GetProcessHeap(), 0, req);
}

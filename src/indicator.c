#include <windows.h>
#include "indicator.h"

#define CLASS_NAME L"HoldSwitchIndicator"
#define TIMER_HOLD 1
#define TIMER_FADE 2

enum { VISIBLE_MS = 900, FADE_MS = 250, FADE_STEP_MS = 16, MAX_ALPHA = 245 };

/* Размеры в пикселях при 100 % масштаба. */
typedef struct { int w, h, font, corner, border; } style;
static const style LARGE = { 64, 40, 20, 12, 2 };
static const style SMALL = { 34, 22, 13, 6, 1 };

static HWND wnd;
static wchar_t label[16];
static int alpha;
static int scale_pct = 100;
static style cur;

static int px(int v) { return MulDiv(v, scale_pct, 100); }

/* Масштаб монитора, на котором появится плашка (Windows 8.1+), иначе системный. */
static void use_monitor_scale(HMONITOR mon)
{
    typedef HRESULT(WINAPI * dpi_fn)(HMONITOR, int, UINT *, UINT *);
    static dpi_fn get_dpi;
    static int looked_up;
    if (!looked_up) {
        HMODULE shcore = LoadLibraryW(L"shcore.dll");
        get_dpi = shcore ? (dpi_fn)(void *)GetProcAddress(shcore, "GetDpiForMonitor") : NULL;
        looked_up = 1;
    }
    UINT dx = 0, dy = 0;
    if (get_dpi && SUCCEEDED(get_dpi(mon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy)) && dy) {
        scale_pct = MulDiv((int)dy, 100, 96);
        return;
    }
    HDC dc = GetDC(NULL);
    scale_pct = MulDiv(GetDeviceCaps(dc, LOGPIXELSY), 100, 96);
    ReleaseDC(NULL, dc);
}

void layout_code(HKL layout, wchar_t *out, int cap)
{
    LANGID lang = LOWORD((ULONG_PTR)layout);
    if (!GetLocaleInfoW(MAKELCID(lang, SORT_DEFAULT), LOCALE_SISO639LANGNAME, out, cap))
        lstrcpynW(out, L"??", cap);
    CharUpperW(out);
}

static void paint(HWND h)
{
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(h, &ps);
    RECT rc;
    GetClientRect(h, &rc);

    HBRUSH bg = CreateSolidBrush(HS_ACCENT);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    /* белая рамка — чтобы плашку было видно и на тёмном, и на синем фоне */
    int corner = px(cur.corner), border = px(cur.border) > 0 ? px(cur.border) : 1;
    HRGN edge = CreateRoundRectRgn(0, 0, rc.right + 1, rc.bottom + 1, corner, corner);
    FrameRgn(dc, edge, (HBRUSH)GetStockObject(WHITE_BRUSH), border, border);
    DeleteObject(edge);

    HFONT font = CreateFontW(-px(cur.font), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ old = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, label, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, old);
    DeleteObject(font);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK proc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_PAINT:
        paint(h);
        return 0;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_TIMER:
        if (wp == TIMER_HOLD) {
            KillTimer(h, TIMER_HOLD);
            SetTimer(h, TIMER_FADE, FADE_STEP_MS, NULL);
        } else if (wp == TIMER_FADE) {
            alpha -= MAX_ALPHA * FADE_STEP_MS / FADE_MS;
            if (alpha <= 0) {
                KillTimer(h, TIMER_FADE);
                ShowWindow(h, SW_HIDE);
            } else {
                SetLayeredWindowAttributes(h, 0, (BYTE)alpha, LWA_ALPHA);
            }
        }
        return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void indicator_init(HINSTANCE inst)
{
    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = proc;
    wc.hInstance = inst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    wnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED |
                              WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
                          CLASS_NAME, L"", WS_POPUP, 0, 0, 1, 1, NULL, NULL, inst, NULL);

    HDC dc = GetDC(NULL);
    scale_pct = MulDiv(GetDeviceCaps(dc, LOGPIXELSY), 100, 96);
    ReleaseDC(NULL, dc);
}

/* Показать плашку стиля st в точке (x, y) с поправкой, чтобы не вылезла за монитор. */
static void show_at(HKL layout, const style *st, int x, int y, const RECT *work)
{
    if (!wnd) return;
    layout_code(layout, label, sizeof label / sizeof label[0]);
    cur = *st;
    int w = px(st->w), hgt = px(st->h);

    if (x + w > work->right) x = work->right - w;
    if (x < work->left) x = work->left;
    if (y + hgt > work->bottom) y = work->bottom - hgt;
    if (y < work->top) y = work->top;

    HRGN rgn = CreateRoundRectRgn(0, 0, w + 1, hgt + 1, px(st->corner), px(st->corner));
    SetWindowRgn(wnd, rgn, FALSE); /* регион теперь принадлежит окну */

    KillTimer(wnd, TIMER_FADE);
    alpha = MAX_ALPHA;
    SetLayeredWindowAttributes(wnd, 0, (BYTE)alpha, LWA_ALPHA);
    SetWindowPos(wnd, HWND_TOPMOST, x, y, w, hgt, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(wnd, NULL, TRUE);
    UpdateWindow(wnd);
    SetTimer(wnd, TIMER_HOLD, VISIBLE_MS, NULL);
}

static RECT work_area(HMONITOR mon)
{
    MONITORINFO mi;
    ZeroMemory(&mi, sizeof mi);
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    return mi.rcWork;
}

void indicator_show(HKL layout, HWND anchor)
{
    HMONITOR mon = MonitorFromWindow(anchor ? anchor : GetDesktopWindow(), MONITOR_DEFAULTTOPRIMARY);
    use_monitor_scale(mon);
    RECT work = work_area(mon);
    int margin = px(16);
    show_at(layout, &LARGE, work.right - px(LARGE.w) - margin,
            work.bottom - px(LARGE.h) - margin, &work);
}

void indicator_show_near(HKL layout, const RECT *caret)
{
    HMONITOR mon = MonitorFromRect(caret, MONITOR_DEFAULTTONEAREST);
    use_monitor_scale(mon);
    RECT work = work_area(mon);
    int gap = px(4), hgt = px(SMALL.h);
    int y = caret->top - hgt - gap;
    if (y < work.top) y = caret->bottom + gap; /* сверху нет места — под курсором */
    show_at(layout, &SMALL, caret->left, y, &work);
}

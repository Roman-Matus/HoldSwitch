#ifndef HS_SWITCHER_H
#define HS_SWITCHER_H

#include <windows.h>
#include "engine.h"

/* Вызывается, когда переключение закончилось (успешно или нет):
 * actual — раскладка окна переднего плана после попытки. */
typedef void (*switch_done_fn)(HKL actual, HWND fg, int type_after, hs_key key);

/* owner получает WM_TIMER с id timer_id — его надо передавать в switcher_on_timer. */
void switcher_init(HWND owner, UINT_PTR timer_id, switch_done_fn done);

void switcher_request(int type_after, hs_key key);

void switcher_on_timer(void);

/* Вставить нажатие клавиши через SendInput с нашей меткой. */
void inject_keys(const hs_key *keys, int count);

#define HS_INJECT_MARK ((ULONG_PTR)0x48535754) /* "HSWT" */

#endif

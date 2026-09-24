#ifndef HS_CARET_H
#define HS_CARET_H

#include <windows.h>

/* Найти курсор ввода в окне fg в отдельном потоке (чужое приложение может
 * отвечать долго, а наш поток держит хук клавиатуры). Результат приходит
 * сообщением msg окну notify: wParam = seq, lParam = RECT* в экранных
 * координатах (освобождает получатель через HeapFree(GetProcessHeap())).
 * Если курсор не найден — там прямоугольник указателя мыши. */
void caret_locate_async(HWND notify, UINT msg, WPARAM seq, HWND fg);

/* Разбудить поддержку специальных возможностей окна fg заранее, в фоне:
 * Qt (Telegram) и Chromium на первый запрос отвечают пустым. */
void caret_warm_up(HWND fg);

#endif

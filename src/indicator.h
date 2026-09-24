#ifndef HS_INDICATOR_H
#define HS_INDICATOR_H

#include <windows.h>

/* Фирменный синий: значок в трее и плашка языка. */
#define HS_ACCENT RGB(45, 108, 223)

void indicator_init(HINSTANCE inst);

/* Показать код языка раскладки в правом нижнем углу монитора,
 * на котором окно anchor; плашка сама гаснет. */
void indicator_show(HKL layout, HWND anchor);

/* Маленькая плашка над прямоугольником курсора ввода (экранные координаты). */
void indicator_show_near(HKL layout, const RECT *caret);

/* Двухбуквенный код языка раскладки: «RU», «EN». */
void layout_code(HKL layout, wchar_t *out, int cap);

#endif

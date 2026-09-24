/* Строки интерфейса на русском и английском. Без Windows API — проверяется тестом. */
#ifndef HS_I18N_H
#define HS_I18N_H

#include <wchar.h>

typedef enum {
    HS_UI_AUTO = 0, /* как в Windows */
    HS_UI_RU = 1,
    HS_UI_EN = 2
} hs_ui_lang;

typedef enum {
    S_TIP_MODE_A,
    S_TIP_MODE_B,
    S_TIP_MODE_C,
    S_TIP_FORMAT, /* %s — режим, %u — мс */
    S_TIP_OFF,
    S_MENU_MODE_A,
    S_MENU_MODE_B,
    S_MENU_MODE_C,
    S_MS_FORMAT, /* %u — мс */
    S_IND_CORNER,
    S_IND_CARET,
    S_IND_OFF,
    S_ENABLED,
    S_ON_HOLD,
    S_DURATION,
    S_SPACE,
    S_INDICATOR,
    S_AUTOSTART,
    S_UI_LANG,
    S_UI_AUTO,
    S_UI_RU,
    S_UI_EN,
    S_ABOUT,
    S_ABOUT_TEXT, /* %s — версия, %s — год, %s — автор */
    S_EXIT,
    S_ALREADY_RUNNING,
    S_HOOK_FAILED,
    S_BALLOON_TITLE,
    S_BALLOON_TEXT,
    S_COUNT
} hs_str;

/* Язык, который получится для настройки ui_lang при языке Windows
 * primary_langid (PRIMARYLANGID от GetUserDefaultUILanguage). */
hs_ui_lang i18n_resolve(int ui_lang, unsigned primary_langid);

void i18n_use(hs_ui_lang resolved);

const wchar_t *tr(hs_str id);

/* Для тестов: строка конкретного языка. */
const wchar_t *tr_in(hs_ui_lang resolved, hs_str id);

#endif

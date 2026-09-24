#include "i18n.h"

enum { LANGID_RUSSIAN = 0x19, LANGID_UKRAINIAN = 0x22, LANGID_BELARUSIAN = 0x23 };

static const wchar_t *const ru[S_COUNT] = {
    [S_TIP_MODE_A] = L"только переключить",
    [S_TIP_MODE_B] = L"переключить и напечатать",
    [S_TIP_MODE_C] = L"напечатать, при удержании стереть",
    [S_TIP_FORMAT] = L"HoldSwitch: %s, %u мс",
    [S_TIP_OFF] = L"HoldSwitch: выключено",
    [S_MENU_MODE_A] = L"А — только переключить язык",
    [S_MENU_MODE_B] = L"Б — переключить и напечатать в новом языке",
    [S_MENU_MODE_C] = L"В — напечатать сразу, при удержании стереть и переключить",
    [S_MS_FORMAT] = L"%u мс",
    [S_IND_CORNER] = L"В правом нижнем углу экрана",
    [S_IND_CARET] = L"Маленький, над курсором ввода",
    [S_IND_OFF] = L"Не показывать",
    [S_ENABLED] = L"Включено",
    [S_ON_HOLD] = L"При долгом нажатии",
    [S_DURATION] = L"Длительность нажатия",
    [S_SPACE] = L"Пробел тоже переключает",
    [S_INDICATOR] = L"Индикатор языка",
    [S_AUTOSTART] = L"Запускать вместе с Windows",
    [S_UI_LANG] = L"Язык / Language",
    [S_UI_AUTO] = L"Как в Windows",
    [S_UI_RU] = L"Русский",
    [S_UI_EN] = L"English",
    [S_ABOUT] = L"О программе",
    [S_ABOUT_TEXT] = L"HoldSwitch %s\nПереключение языка долгим нажатием клавиши.\n\n© %s %s. Лицензия MIT.",
    [S_EXIT] = L"Выход",
    [S_ALREADY_RUNNING] = L"HoldSwitch уже запущен — значок в области уведомлений.",
    [S_HOOK_FAILED] = L"Не удалось перехватить клавиатуру.",
    [S_BALLOON_TITLE] = L"HoldSwitch работает",
    [S_BALLOON_TEXT] = L"Долгое нажатие буквы переключает язык. Настройки — в меню этого значка.",
};

static const wchar_t *const en[S_COUNT] = {
    [S_TIP_MODE_A] = L"switch only",
    [S_TIP_MODE_B] = L"switch and type",
    [S_TIP_MODE_C] = L"type, erase on hold",
    [S_TIP_FORMAT] = L"HoldSwitch: %s, %u ms",
    [S_TIP_OFF] = L"HoldSwitch: off",
    [S_MENU_MODE_A] = L"A — only switch the language",
    [S_MENU_MODE_B] = L"B — switch and type in the new language",
    [S_MENU_MODE_C] = L"C — type at once; on hold, erase and switch",
    [S_MS_FORMAT] = L"%u ms",
    [S_IND_CORNER] = L"Bottom-right corner of the screen",
    [S_IND_CARET] = L"Small, above the text cursor",
    [S_IND_OFF] = L"Don't show",
    [S_ENABLED] = L"Enabled",
    [S_ON_HOLD] = L"On long press",
    [S_DURATION] = L"Hold time",
    [S_SPACE] = L"Space bar switches too",
    [S_INDICATOR] = L"Language indicator",
    [S_AUTOSTART] = L"Start with Windows",
    [S_UI_LANG] = L"Язык / Language",
    [S_UI_AUTO] = L"Same as Windows",
    [S_UI_RU] = L"Русский",
    [S_UI_EN] = L"English",
    [S_ABOUT] = L"About",
    [S_ABOUT_TEXT] = L"HoldSwitch %s\nSwitch the input language with a long key press.\n\n© %s %s. MIT License.",
    [S_EXIT] = L"Exit",
    [S_ALREADY_RUNNING] = L"HoldSwitch is already running — see its icon in the notification area.",
    [S_HOOK_FAILED] = L"Could not start listening to the keyboard.",
    [S_BALLOON_TITLE] = L"HoldSwitch is running",
    [S_BALLOON_TEXT] = L"Hold a letter key to switch the input language. Settings are in this icon's menu.",
};

static hs_ui_lang current = HS_UI_RU;

hs_ui_lang i18n_resolve(int ui_lang, unsigned primary_langid)
{
    if (ui_lang == HS_UI_RU || ui_lang == HS_UI_EN) return (hs_ui_lang)ui_lang;
    switch (primary_langid) {
    case LANGID_RUSSIAN: case LANGID_UKRAINIAN: case LANGID_BELARUSIAN: return HS_UI_RU;
    default: return HS_UI_EN;
    }
}

void i18n_use(hs_ui_lang resolved)
{
    current = resolved == HS_UI_EN ? HS_UI_EN : HS_UI_RU;
}

const wchar_t *tr_in(hs_ui_lang resolved, hs_str id)
{
    if ((unsigned)id >= S_COUNT) return L"";
    const wchar_t *s = (resolved == HS_UI_EN ? en : ru)[id];
    return s ? s : L"";
}

const wchar_t *tr(hs_str id)
{
    return tr_in(current, id);
}

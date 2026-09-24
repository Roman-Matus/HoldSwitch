#ifndef HS_SETTINGS_H
#define HS_SETTINGS_H

typedef enum {
    HS_INDICATOR_OFF = 0,
    HS_INDICATOR_CORNER = 1, /* в правом нижнем углу экрана */
    HS_INDICATOR_CARET = 2   /* маленький, над курсором ввода */
} hs_indicator;

typedef struct {
    int enabled;
    int mode;              /* hs_mode */
    unsigned threshold_ms;
    int include_space;
    int indicator;         /* hs_indicator */
    int ui_lang;           /* hs_ui_lang */
} hs_settings;

/* Возвращает 0, если файла настроек ещё не было (первый запуск). */
int settings_load(hs_settings *s);
void settings_save(const hs_settings *s);

int autostart_get(void);
void autostart_set(int on);

#endif

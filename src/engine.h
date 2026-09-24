/* Логика долгого нажатия без Windows API.
 *
 * Платформа кормит движок событиями клавиатуры из хука и тиками таймера
 * удержания; движок отвечает, пропустить событие или подавить, и какие
 * действия выполнить (вставить клавиши, запустить/остановить таймер,
 * переключить раскладку). */
#ifndef HS_ENGINE_H
#define HS_ENGINE_H

typedef enum {
    HS_MODE_SWITCH_ONLY = 0,     /* А: удержание только переключает */
    HS_MODE_SWITCH_AND_TYPE = 1, /* Б: переключает и печатает в новой раскладке */
    HS_MODE_TYPE_THEN_ERASE = 2  /* В: печатает сразу, при удержании стирает */
} hs_mode;

typedef struct {
    unsigned short vk;
    unsigned short scan;
    unsigned char extended;
    unsigned char up;
} hs_key;

typedef enum {
    HS_ACT_INJECT,      /* вставить key */
    HS_ACT_TIMER_START, /* (пере)запустить таймер удержания на ms */
    HS_ACT_TIMER_STOP,
    HS_ACT_SWITCH       /* переключить раскладку; если type_after — потом напечатать key */
} hs_action_kind;

typedef struct {
    hs_action_kind kind;
    hs_key key;
    int type_after;
    unsigned ms;
} hs_action;

#define HS_MAX_ACTIONS 8

typedef struct {
    int suppress;
    int count;
    hs_action actions[HS_MAX_ACTIONS];
} hs_result;

typedef struct {
    /* настройки */
    hs_mode mode;
    unsigned threshold_ms;
    int include_space;
    int enabled;

    /* состояние */
    unsigned char down[256];       /* физически нажата (по нашим наблюдениям) */
    unsigned char mod_down[256];   /* модификаторы, включая вставленные события */
    unsigned char ours[256];       /* нажатие взято под контроль: повторы гасим */
    unsigned char swallow_up[256]; /* отпускание надо подавить: клавишу уже напечатали мы */
    int hold_active;
    int hold_fired;
    hs_key hold_key;
} hs_engine;

void hs_init(hs_engine *e);

/* Забыть всё состояние без действий (смена сеанса: отпускания могли потеряться). */
void hs_reset(hs_engine *e);

/* Завершить текущее удержание как обычное нажатие — перед сменой настроек. */
hs_result hs_flush(hs_engine *e);

hs_result hs_on_key(hs_engine *e, hs_key key, int injected);

hs_result hs_on_timer(hs_engine *e);

int hs_is_printable(unsigned vk, int include_space);

#endif

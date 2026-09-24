#include <string.h>
#include "engine.h"

enum {
    VK_BACK = 0x08,
    VK_CONTROL = 0x11, VK_MENU = 0x12,
    VK_SPACE = 0x20,
    VK_LWIN = 0x5B, VK_RWIN = 0x5C,
    VK_LCONTROL = 0xA2, VK_RCONTROL = 0xA3, VK_LMENU = 0xA4, VK_RMENU = 0xA5
};

void hs_init(hs_engine *e)
{
    memset(e, 0, sizeof *e);
    e->mode = HS_MODE_SWITCH_ONLY;
    e->threshold_ms = 400;
    e->enabled = 1;
}

void hs_reset(hs_engine *e)
{
    memset(e->down, 0, sizeof e->down);
    memset(e->mod_down, 0, sizeof e->mod_down);
    memset(e->ours, 0, sizeof e->ours);
    memset(e->swallow_up, 0, sizeof e->swallow_up);
    e->hold_active = 0;
    e->hold_fired = 0;
}

int hs_is_printable(unsigned vk, int include_space)
{
    if (vk >= '0' && vk <= '9') return 1;
    if (vk >= 'A' && vk <= 'Z') return 1;
    if (vk >= 0x60 && vk <= 0x6F) return 1; /* цифровой блок при NumLock */
    if (vk >= 0xBA && vk <= 0xC0) return 1; /* ; = , - . / ` */
    if (vk >= 0xDB && vk <= 0xDF) return 1; /* [ \ ] ' OEM_8 */
    if (vk == 0xE2) return 1;               /* OEM_102 */
    if (vk == VK_SPACE) return include_space;
    return 0;
}

static int is_modifier(unsigned vk)
{
    switch (vk) {
    case VK_CONTROL: case VK_MENU: case VK_LWIN: case VK_RWIN:
    case VK_LCONTROL: case VK_RCONTROL: case VK_LMENU: case VK_RMENU:
        return 1;
    }
    return 0;
}

/* Ctrl, Alt или Win зажаты — значит это сочетание, а не печать. */
static int combo_held(const hs_engine *e)
{
    return e->mod_down[VK_CONTROL] || e->mod_down[VK_MENU] ||
           e->mod_down[VK_LWIN] || e->mod_down[VK_RWIN] ||
           e->mod_down[VK_LCONTROL] || e->mod_down[VK_RCONTROL] ||
           e->mod_down[VK_LMENU] || e->mod_down[VK_RMENU];
}

static int deferred_mode(const hs_engine *e)
{
    return e->mode != HS_MODE_TYPE_THEN_ERASE;
}

static void push(hs_result *r, hs_action a)
{
    if (r->count < HS_MAX_ACTIONS) r->actions[r->count++] = a;
}

static void push_inject(hs_result *r, hs_key k)
{
    hs_action a = { HS_ACT_INJECT, k, 0, 0 };
    push(r, a);
}

static void push_tap(hs_result *r, hs_key k)
{
    k.up = 0; push_inject(r, k);
    k.up = 1; push_inject(r, k);
}

static void push_timer(hs_result *r, hs_action_kind kind, unsigned ms)
{
    hs_action a = { kind, { 0, 0, 0, 0 }, 0, ms };
    push(r, a);
}

/* Закончить удержание, пока его клавиша ещё физически нажата: её
 * отпускание дальше не наше дело. В режимах А/Б несработавшее удержание
 * печатается (отложенное нажатие выдаём сейчас). */
static void end_hold_early(hs_engine *e, hs_result *r)
{
    if (!e->hold_active) return;
    unsigned vk = e->hold_key.vk;
    if (!e->hold_fired) {
        push_timer(r, HS_ACT_TIMER_STOP, 0);
        if (deferred_mode(e)) push_tap(r, e->hold_key);
    }
    if (deferred_mode(e)) e->swallow_up[vk] = 1;
    e->hold_active = 0;
    e->hold_fired = 0;
}

hs_result hs_flush(hs_engine *e)
{
    hs_result r = { 0, 0, { { 0 } } };
    end_hold_early(e, &r);
    return r;
}

static hs_result on_hold_key_up(hs_engine *e, hs_key k)
{
    hs_result r = { 0, 0, { { 0 } } };
    if (!e->hold_fired) {
        push_timer(&r, HS_ACT_TIMER_STOP, 0);
        if (deferred_mode(e)) push_tap(&r, k);
    }
    r.suppress = deferred_mode(e);
    e->hold_active = 0;
    e->hold_fired = 0;
    return r;
}

static hs_result on_printable_down(hs_engine *e, hs_key k)
{
    hs_result r = { 0, 0, { { 0 } } };
    end_hold_early(e, &r);
    e->hold_active = 1;
    e->hold_fired = 0;
    e->hold_key = k;
    e->ours[k.vk] = 1;
    push_timer(&r, HS_ACT_TIMER_START, e->threshold_ms);
    r.suppress = deferred_mode(e);
    return r;
}

/* Любое прочее событие. В режимах А/Б отложенную клавишу надо выдать
 * раньше него, поэтому само событие подавляем и вставляем следом. */
static hs_result on_other(hs_engine *e, hs_key k)
{
    hs_result r = { 0, 0, { { 0 } } };
    if (!e->hold_active) return r;
    if (!deferred_mode(e)) {
        if (!k.up) end_hold_early(e, &r); /* отпускания других клавиш удержание не прерывают */
        return r;
    }
    int must_reorder = !e->hold_fired;
    end_hold_early(e, &r);
    if (must_reorder) {
        push_inject(&r, k);
        r.suppress = 1;
    }
    return r;
}

hs_result hs_on_key(hs_engine *e, hs_key k, int injected)
{
    hs_result none = { 0, 0, { { 0 } } };
    unsigned vk = k.vk & 0xFF;

    if (is_modifier(vk)) e->mod_down[vk] = !k.up;
    if (injected) return none;

    int was_down = e->down[vk];
    e->down[vk] = !k.up;

    if (k.up) {
        if (e->swallow_up[vk]) {
            e->swallow_up[vk] = 0;
            e->ours[vk] = 0;
            none.suppress = 1;
            return none;
        }
        if (e->hold_active && e->hold_key.vk == vk) {
            e->ours[vk] = 0;
            return on_hold_key_up(e, k);
        }
        e->ours[vk] = 0;
        if (!e->enabled) return none;
        return on_other(e, k);
    }

    if (was_down) { /* автоповтор */
        if (e->ours[vk]) none.suppress = 1;
        return none;
    }
    if (!e->enabled) return none;
    if (hs_is_printable(vk, e->include_space) && !combo_held(e))
        return on_printable_down(e, k);
    return on_other(e, k);
}

hs_result hs_on_timer(hs_engine *e)
{
    hs_result r = { 0, 0, { { 0 } } };
    if (!e->enabled || !e->hold_active || e->hold_fired) return r;
    e->hold_fired = 1;
    if (e->mode == HS_MODE_TYPE_THEN_ERASE) {
        hs_key back = { VK_BACK, 0x0E, 0, 0 };
        push_tap(&r, back);
    }
    hs_action sw = { HS_ACT_SWITCH, e->hold_key, e->mode == HS_MODE_SWITCH_AND_TYPE, 0 };
    sw.key.up = 0;
    push(&r, sw);
    return r;
}

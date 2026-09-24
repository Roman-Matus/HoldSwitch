/* Тесты движка на симуляторе: «система» с автоповтором, таймером,
 * двумя раскладками (EN/RU) и приложением, которое печатает текст. */
#include <stdio.h>
#include <string.h>
#include "../src/engine.h"

#define VK_BACK 0x08
#define VK_RETURN 0x0D
#define VK_SPACE 0x20
#define VK_LSHIFT 0xA0
#define VK_LCONTROL 0xA2

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s:%d: ", __func__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------- симулятор ---------- */

static hs_engine eng;
static char text[256];
static int layout_ru;
static int switches;
static int app_shift;
static unsigned now_ms;
static int timer_on;
static unsigned timer_deadline;
static int phys_down[256];
static int last_down_vk; /* автоповтор идёт только у последней нажатой клавиши */
static unsigned repeat_next;

enum { REPEAT_DELAY = 500, REPEAT_RATE = 33 };

static const char *ru_letter(unsigned vk, int upper)
{
    switch (vk) {
    case 'F': return upper ? "А" : "а";
    case 'D': return upper ? "В" : "в";
    case 'G': return upper ? "П" : "п";
    default: return "?";
    }
}

static void app_receive(hs_key k)
{
    if (k.vk == VK_LSHIFT) { app_shift = !k.up; return; }
    if (k.up) return;
    size_t n = strlen(text);
    if (k.vk == VK_BACK) {
        if (n == 0) return;
        n--;
        while (n > 0 && (text[n] & 0xC0) == 0x80) n--; /* UTF-8 */
        text[n] = 0;
        return;
    }
    if (k.vk == VK_RETURN) { strcat(text, "\n"); return; }
    if (k.vk == VK_SPACE) { strcat(text, " "); return; }
    if (k.vk >= 'A' && k.vk <= 'Z') {
        if (layout_ru) { strcat(text, ru_letter(k.vk, app_shift)); return; }
        text[n] = (char)(app_shift ? k.vk : k.vk + 32);
        text[n + 1] = 0;
        return;
    }
    if (k.vk >= '0' && k.vk <= '9') { text[n] = (char)k.vk; text[n + 1] = 0; }
}

static void deliver(hs_key k, int injected);

static void run_actions(hs_result r)
{
    for (int i = 0; i < r.count; i++) {
        hs_action a = r.actions[i];
        switch (a.kind) {
        case HS_ACT_INJECT: deliver(a.key, 1); break;
        case HS_ACT_TIMER_START: timer_on = 1; timer_deadline = now_ms + a.ms; break;
        case HS_ACT_TIMER_STOP: timer_on = 0; break;
        case HS_ACT_SWITCH: {
            layout_ru = !layout_ru;
            switches++;
            if (a.type_after) {
                hs_key k = a.key;
                k.up = 0; deliver(k, 1);
                k.up = 1; deliver(k, 1);
            }
            break;
        }
        }
    }
}

static void deliver(hs_key k, int injected)
{
    hs_result r = hs_on_key(&eng, k, injected);
    if (!r.suppress) app_receive(k);
    run_actions(r);
}

static hs_key key(unsigned vk, int up)
{
    hs_key k = { (unsigned short)vk, (unsigned short)(vk & 0x7F), 0, (unsigned char)up };
    return k;
}

static void sim_start(hs_mode mode)
{
    hs_init(&eng);
    eng.mode = mode;
    text[0] = 0;
    layout_ru = 0; switches = 0; app_shift = 0; now_ms = 0;
    timer_on = 0; last_down_vk = 0;
    memset(phys_down, 0, sizeof phys_down);
}

static void down(unsigned vk)
{
    phys_down[vk] = 1;
    last_down_vk = vk;
    repeat_next = now_ms + REPEAT_DELAY;
    deliver(key(vk, 0), 0);
}

static void up(unsigned vk)
{
    phys_down[vk] = 0;
    if (last_down_vk == (int)vk) last_down_vk = 0;
    deliver(key(vk, 1), 0);
}

static void wait_ms(unsigned ms)
{
    unsigned end = now_ms + ms;
    while (now_ms < end) {
        now_ms++;
        if (timer_on && now_ms >= timer_deadline) {
            timer_on = 0; /* платформа делает таймер однократным */
            run_actions(hs_on_timer(&eng));
        }
        if (last_down_vk && phys_down[last_down_vk] && now_ms >= repeat_next) {
            repeat_next = now_ms + REPEAT_RATE;
            deliver(key(last_down_vk, 0), 0);
        }
    }
}

static void tap(unsigned vk) { down(vk); wait_ms(60); up(vk); wait_ms(40); }

/* ---------- тесты ---------- */

static void test_printable_classification(void)
{
    CHECK(hs_is_printable('A', 0), "A");
    CHECK(hs_is_printable('7', 0), "7");
    CHECK(hs_is_printable(0xBA, 0), "OEM_1");
    CHECK(hs_is_printable(0xC0, 0), "OEM_3");
    CHECK(hs_is_printable(0xDE, 0), "OEM_7");
    CHECK(hs_is_printable(0xE2, 0), "OEM_102");
    CHECK(hs_is_printable(0x60, 0), "NUMPAD0");
    CHECK(hs_is_printable(0x6F, 0), "DIVIDE");
    CHECK(!hs_is_printable(VK_SPACE, 0), "space off");
    CHECK(hs_is_printable(VK_SPACE, 1), "space on");
    CHECK(!hs_is_printable(VK_RETURN, 1), "enter");
    CHECK(!hs_is_printable(VK_BACK, 1), "backspace");
    CHECK(!hs_is_printable(VK_LSHIFT, 1), "shift");
    CHECK(!hs_is_printable(0x25, 1), "left arrow");
    CHECK(!hs_is_printable(0x70, 1), "F1");
}

static void test_short_taps_type_normally_in_all_modes(void)
{
    for (int m = 0; m < 3; m++) {
        sim_start((hs_mode)m);
        tap('F'); tap('D'); tap('1');
        CHECK(strcmp(text, "fd1") == 0, "mode %d: text '%s'", m, text);
        CHECK(switches == 0, "mode %d: switches %d", m, switches);
    }
}

static void test_mode_a_long_press_switches_without_typing(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    tap('D');
    down('F'); wait_ms(1500); up('F');
    CHECK(strcmp(text, "d") == 0, "text '%s'", text);
    CHECK(switches == 1, "switches %d", switches);
    tap('F');
    CHECK(strcmp(text, "dа") == 0, "after switch '%s'", text);
}

static void test_mode_b_long_press_switches_and_types_in_new_layout(void)
{
    sim_start(HS_MODE_SWITCH_AND_TYPE);
    down('F'); wait_ms(1500); up('F');
    CHECK(strcmp(text, "а") == 0, "text '%s'", text);
    CHECK(switches == 1, "switches %d", switches);
}

static void test_mode_c_types_then_erases(void)
{
    sim_start(HS_MODE_TYPE_THEN_ERASE);
    tap('D');
    down('F');
    CHECK(strcmp(text, "df") == 0, "typed immediately '%s'", text);
    wait_ms(1500); up('F');
    CHECK(strcmp(text, "d") == 0, "erased '%s'", text);
    CHECK(switches == 1, "switches %d", switches);
}

static void test_one_switch_per_hold(void)
{
    for (int m = 0; m < 3; m++) {
        sim_start((hs_mode)m);
        down('G'); wait_ms(5000); up('G');
        CHECK(switches == 1, "mode %d: switches %d", m, switches);
    }
}

static void test_threshold_boundary(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    eng.threshold_ms = 400;
    down('F'); wait_ms(399); up('F');
    CHECK(switches == 0 && strcmp(text, "f") == 0, "below: sw %d '%s'", switches, text);
    down('F'); wait_ms(400); up('F');
    CHECK(switches == 1 && strcmp(text, "f") == 0, "at: sw %d '%s'", switches, text);
}

static void test_rollover_keeps_order(void)
{
    for (int m = 0; m < 3; m++) {
        sim_start((hs_mode)m);
        /* D↓ F↓ D↑ G↓ F↑ G↑ — быстрый набор с перекрытием */
        down('D'); wait_ms(30);
        down('F'); wait_ms(30);
        up('D'); wait_ms(10);
        down('G'); wait_ms(20);
        up('F'); wait_ms(40);
        up('G');
        CHECK(strcmp(text, "dfg") == 0, "mode %d: '%s'", m, text);
        CHECK(switches == 0, "mode %d: switches %d", m, switches);
    }
}

static void test_shift_released_before_letter_keeps_capital(void)
{
    for (int m = 0; m < 3; m++) {
        sim_start((hs_mode)m);
        down(VK_LSHIFT); wait_ms(20);
        down('F'); wait_ms(20);
        up(VK_LSHIFT); wait_ms(20);
        up('F');
        CHECK(strcmp(text, "F") == 0, "mode %d: '%s'", m, text);
    }
}

static void test_shift_long_press_in_mode_b_types_capital_in_new_layout(void)
{
    sim_start(HS_MODE_SWITCH_AND_TYPE);
    down(VK_LSHIFT);
    down('F'); wait_ms(1000); up('F');
    up(VK_LSHIFT);
    CHECK(strcmp(text, "А") == 0, "'%s'", text);
}

static void test_ctrl_combos_pass_through_with_repeat(void)
{
    for (int m = 0; m < 3; m++) {
        sim_start((hs_mode)m);
        hs_result r;
        down(VK_LCONTROL);
        r = hs_on_key(&eng, key('Z', 0), 0);
        CHECK(!r.suppress && r.count == 0, "mode %d: ctrl+z down", m);
        r = hs_on_key(&eng, key('Z', 0), 0); /* автоповтор */
        CHECK(!r.suppress, "mode %d: ctrl+z repeat suppressed", m);
        r = hs_on_key(&eng, key('Z', 1), 0);
        CHECK(!r.suppress, "mode %d: ctrl+z up", m);
        up(VK_LCONTROL);
        CHECK(switches == 0, "mode %d: switches %d", m, switches);
    }
}

static void test_non_printable_keys_keep_autorepeat(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    tap('F'); tap('D'); tap('G');
    down(VK_BACK); wait_ms(600); up(VK_BACK); /* 1 + повторы */
    CHECK(strcmp(text, "") == 0, "'%s'", text);
    CHECK(switches == 0, "switches %d", switches);
}

static void test_enter_while_pending_flushes_first(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    down('F'); wait_ms(20);
    down(VK_RETURN); wait_ms(20);
    up('F'); up(VK_RETURN);
    CHECK(strcmp(text, "f\n") == 0, "'%s'", text);
}

static void test_space_optional(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    down(VK_SPACE); wait_ms(1000); up(VK_SPACE);
    CHECK(switches == 0, "space off: switches %d", switches);
    CHECK(strlen(text) > 1, "space off: repeats kept '%s'", text);

    sim_start(HS_MODE_SWITCH_ONLY);
    eng.include_space = 1;
    tap(VK_SPACE);
    down(VK_SPACE); wait_ms(1000); up(VK_SPACE);
    CHECK(switches == 1 && strcmp(text, " ") == 0, "space on: sw %d '%s'", switches, text);
}

static void test_disabled_passes_everything(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    eng.enabled = 0;
    down('F'); wait_ms(1000); up('F');
    CHECK(switches == 0, "switches %d", switches);
    CHECK(strlen(text) > 1 && text[0] == 'f', "autorepeat back '%s'", text);
}

static void test_flush_before_settings_change(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    down('F'); wait_ms(50);
    run_actions(hs_flush(&eng));
    eng.enabled = 0;
    CHECK(strcmp(text, "f") == 0, "flushed '%s'", text);
    CHECK(!timer_on, "timer stopped");
    up('F');
    CHECK(strcmp(text, "f") == 0, "up swallowed '%s'", text);
}

static void test_reset_forgets_lost_keys(void)
{
    sim_start(HS_MODE_SWITCH_ONLY);
    down(VK_LCONTROL); /* отпускание потерялось на защищённом рабочем столе */
    phys_down[VK_LCONTROL] = 0; last_down_vk = 0;
    hs_reset(&eng);
    down('F'); wait_ms(1000); up('F');
    CHECK(switches == 1, "switches %d", switches);
}

static void test_long_press_after_rollover_still_switches(void)
{
    sim_start(HS_MODE_TYPE_THEN_ERASE);
    down('D'); wait_ms(30);
    down('F'); wait_ms(30);
    up('D');                 /* отпускание предыдущей не отменяет удержание F */
    wait_ms(1000); up('F');
    CHECK(switches == 1, "switches %d", switches);
    CHECK(strcmp(text, "d") == 0, "'%s'", text);
}

int main(void)
{
    test_printable_classification();
    test_short_taps_type_normally_in_all_modes();
    test_mode_a_long_press_switches_without_typing();
    test_mode_b_long_press_switches_and_types_in_new_layout();
    test_mode_c_types_then_erases();
    test_one_switch_per_hold();
    test_threshold_boundary();
    test_rollover_keeps_order();
    test_shift_released_before_letter_keeps_capital();
    test_shift_long_press_in_mode_b_types_capital_in_new_layout();
    test_ctrl_combos_pass_through_with_repeat();
    test_non_printable_keys_keep_autorepeat();
    test_enter_while_pending_flushes_first();
    test_space_optional();
    test_disabled_passes_everything();
    test_flush_before_settings_change();
    test_reset_forgets_lost_keys();
    test_long_press_after_rollover_still_switches();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

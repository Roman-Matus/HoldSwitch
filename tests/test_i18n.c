/* Каждая строка есть на обоих языках, подстановки в шаблонах совпадают,
 * язык «как в Windows» выбирается правильно. */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "../src/i18n.h"

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL %s:%d: ", __func__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Последовательность спецификаторов: "%s%u" для "HoldSwitch: %s, %u мс". */
static void specs(const wchar_t *s, char *out, int cap)
{
    int n = 0;
    for (; *s && n < cap - 3; s++) {
        if (*s != L'%') continue;
        s++;
        if (!*s) break;
        out[n++] = '%';
        out[n++] = (char)*s;
    }
    out[n] = 0;
}

static void test_every_string_present_in_both_languages(void)
{
    for (int id = 0; id < S_COUNT; id++) {
        CHECK(wcslen(tr_in(HS_UI_RU, (hs_str)id)) > 0, "ru string %d missing", id);
        CHECK(wcslen(tr_in(HS_UI_EN, (hs_str)id)) > 0, "en string %d missing", id);
    }
}

static void test_format_specifiers_match(void)
{
    for (int id = 0; id < S_COUNT; id++) {
        char a[32], b[32];
        specs(tr_in(HS_UI_RU, (hs_str)id), a, sizeof a);
        specs(tr_in(HS_UI_EN, (hs_str)id), b, sizeof b);
        CHECK(!strcmp(a, b), "string %d: ru '%s' vs en '%s'", id, a, b);
    }
}

static void test_english_has_no_cyrillic_except_language_names(void)
{
    for (int id = 0; id < S_COUNT; id++) {
        if (id == S_UI_LANG || id == S_UI_RU) continue; /* подписи для поиска своего языка */
        for (const wchar_t *s = tr_in(HS_UI_EN, (hs_str)id); *s; s++)
            if (*s >= 0x400 && *s <= 0x4FF) {
                CHECK(0, "en string %d contains Cyrillic", id);
                break;
            }
    }
}

static void test_resolve(void)
{
    CHECK(i18n_resolve(HS_UI_AUTO, 0x19) == HS_UI_RU, "ru windows -> ru");
    CHECK(i18n_resolve(HS_UI_AUTO, 0x22) == HS_UI_RU, "uk windows -> ru");
    CHECK(i18n_resolve(HS_UI_AUTO, 0x23) == HS_UI_RU, "be windows -> ru");
    CHECK(i18n_resolve(HS_UI_AUTO, 0x09) == HS_UI_EN, "en windows -> en");
    CHECK(i18n_resolve(HS_UI_AUTO, 0x07) == HS_UI_EN, "de windows -> en");
    CHECK(i18n_resolve(HS_UI_EN, 0x19) == HS_UI_EN, "explicit en wins");
    CHECK(i18n_resolve(HS_UI_RU, 0x09) == HS_UI_RU, "explicit ru wins");
}

static void test_switching_language(void)
{
    i18n_use(HS_UI_EN);
    CHECK(!wcscmp(tr(S_EXIT), L"Exit"), "en exit");
    i18n_use(HS_UI_RU);
    CHECK(!wcscmp(tr(S_EXIT), L"Выход"), "ru exit");
    CHECK(wcslen(tr((hs_str)S_COUNT)) == 0, "out of range is empty");
}

int main(void)
{
    test_every_string_present_in_both_languages();
    test_format_specifiers_match();
    test_english_has_no_cyrillic_except_language_names();
    test_resolve();
    test_switching_language();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

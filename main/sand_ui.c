#include "sand_ui.h"

#include "sand_copy.h"

#include "lvgl.h"
#include "misc/cache/instance/lv_image_cache.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_montserrat_14);
LV_FONT_DECLARE(sand_font_16);
LV_FONT_DECLARE(sand_font_32);
LV_FONT_DECLARE(sand_font_32);
LV_FONT_DECLARE(sand_font_64);

#define COL_BG     0x14110E
#define COL_INK    0xF7F1E6
#define COL_MUTED  0xB3A79A
#define COL_VIAL   0x2C261F
#define COL_ISLAND 0x100E0C
#define COL_BAT_LOW 0xE07A5F

#define VIAL_W 68
#define VIAL_H 158
#define VIAL_Y 56
#define VIAL_GAP 8

#define UI_MENU 1
#define UI_PICK 2
#define UI_SETUP 3
#define UI_FALL 4
#define UI_RISE 5
#define UI_FORM 6
#define UI_LETTER 7

#define CARD_X 16
#define CARD_W 208
#define CARD_H 50
#define CARD_Y0 34
#define CARD_GAP 4
#define CARD_MOTE_COUNT 5

#define ROW_X 16
#define ROW_W 208
#define ROW_H 46
#define ROW_Y0 48
#define ROW_GAP 6

static lv_obj_t *s_scr;
static int s_mode;

static lv_obj_t *s_hint;
static lv_obj_t *s_hint_color;
static lv_obj_t *s_swatch;
static lv_obj_t *s_time;
static lv_obj_t *s_form;

typedef struct {
    lv_obj_t *shell;
    lv_obj_t *pile;
    lv_obj_t *num;
    lv_obj_t *unit;
} vial_ui_t;

static vial_ui_t s_vial[SAND_PRESET_COUNT];

typedef struct {
    lv_obj_t *shell;
    lv_obj_t *pile;
    lv_obj_t *name;
    lv_obj_t *tag;
    lv_obj_t *ring;
    lv_obj_t *hand_h;
    lv_obj_t *hand_m;
    lv_obj_t *mote[CARD_MOTE_COUNT];
} menu_card_t;

static menu_card_t s_card[SAND_MENU_COUNT];
/* Right of the word, so the rising dots never cover "Stopwatch". */
static const int RISE_X[CARD_MOTE_COUNT] = {158, 172, 186, 166, 180};
static const int RISE_Y[CARD_MOTE_COUNT] = {8, 16, 12, 28, 32};
static const int RISE_S[CARD_MOTE_COUNT] = {4, 5, 4, 5, 4};
#define MARK_D 26
#define MARK_X (CARD_W - 16 - MARK_D)
#define MARK_Y ((CARD_H - MARK_D) / 2)
static const char *MENU_NAME[SAND_MENU_COUNT] = {
    SAND_STR_FORM, SAND_STR_LETTER, SAND_STR_POMODORO, SAND_STR_STOPWATCH
};

typedef struct {
    lv_obj_t *shell;
    lv_obj_t *key;
    lv_obj_t *value;
    lv_obj_t *swatch;
} setup_row_t;

static setup_row_t s_row[SAND_LROW_COUNT];

static lv_obj_t *s_fill;
static lv_obj_t *s_crest;
static lv_obj_t *s_grains[SAND_GRAIN_COUNT];
static lv_obj_t *s_bat_pct;
static lv_obj_t *s_over_time;
static lv_obj_t *s_over_word;
static bool s_over_shown;

static int s_last_soc = -2;
static int s_last_color = -1;
static int s_last_preset = -1;
static int s_last_menu = -1;
static int s_last_fill = -1;
static int s_last_row = -1;
static int s_last_choice = -1;
static int s_last_form_time = -1;
static char s_last_clock[8];
static char s_last_form[16];
static lv_obj_t *s_canvas;
static int s_canvas_color = -1;
static bool s_form_ready;

LV_DRAW_BUF_DEFINE_STATIC(form_dots, SAND_SCREEN_W, SAND_SCREEN_H, LV_COLOR_FORMAT_I4);
LV_DRAW_BUF_DEFINE_STATIC(settled_dots, SAND_SCREEN_W, SAND_SCREEN_H, LV_COLOR_FORMAT_I4);
static int s_grain_x[SAND_GRAIN_COUNT];
static int s_grain_y[SAND_GRAIN_COUNT];
static int s_grain_shade[SAND_GRAIN_COUNT];
static int s_grain_size[SAND_GRAIN_COUNT];

static const char *PRESET_NUM[SAND_PRESET_COUNT] = {"10", "20", "30"};

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

static lv_obj_t *ring(lv_obj_t *parent, int x, int y, int diameter, uint32_t color)
{
    lv_obj_t *obj = box(parent, x, y, diameter, diameter, color, LV_RADIUS_CIRCLE);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(obj, text);
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    return obj;
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    return scr;
}

static void load_screen(lv_obj_t *scr)
{
    lv_obj_t *previous = s_scr;
    s_scr = scr;
    lv_screen_load(scr);
    if (previous) lv_obj_delete(previous);
}

static void reset_cache(void)
{
    s_last_soc = -2;
    s_last_color = -1;
    s_last_preset = -1;
    s_last_menu = -1;
    s_last_fill = -1;
    s_last_row = -1;
    s_last_choice = -1;
    s_last_form_time = -1;
    s_last_clock[0] = '\0';
    s_last_form[0] = '\0';
    s_over_shown = false;
    s_canvas = NULL;
    s_canvas_color = -1;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        s_grain_x[i] = -10000;
        s_grain_y[i] = -10000;
        s_grain_shade[i] = -1;
        s_grain_size[i] = -1;
    }
}

static void style_readout(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(COL_ISLAND), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(obj, 6, 0);
    lv_obj_set_style_pad_ver(obj, 3, 0);
    lv_obj_set_style_radius(obj, 8, 0);
}

static void sync_battery(int soc)
{
    if (soc == s_last_soc) return;
    s_last_soc = soc;
    if (s_bat_pct) {
        char text[8];
        sand_format_soc(soc, text, sizeof(text));
        if (text[0] == '\0') {
            lv_obj_add_flag(s_bat_pct, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        lv_label_set_text(s_bat_pct, text);
        lv_obj_remove_flag(s_bat_pct, LV_OBJ_FLAG_HIDDEN);
        uint32_t color = soc <= 15 ? COL_BAT_LOW : COL_INK;
        lv_obj_set_style_text_color(s_bat_pct, lv_color_hex(color), 0);
    }
}

static void style_hint(lv_obj_t *obj, int width)
{
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
}

static void clear_cards(void)
{
    for (int i = 0; i < SAND_MENU_COUNT; i++) {
        s_card[i].shell = NULL;
        s_card[i].pile = NULL;
        s_card[i].name = NULL;
        s_card[i].tag = NULL;
        s_card[i].ring = NULL;
        s_card[i].hand_h = NULL;
        s_card[i].hand_m = NULL;
        for (int m = 0; m < CARD_MOTE_COUNT; m++) s_card[i].mote[m] = NULL;
    }
}

static void clear_rows(void)
{
    for (int i = 0; i < SAND_LROW_COUNT; i++) {
        s_row[i].shell = NULL;
        s_row[i].key = NULL;
        s_row[i].value = NULL;
        s_row[i].swatch = NULL;
    }
}

static void build_menu(void)
{
    reset_cache();
    clear_cards();
    clear_rows();
    s_swatch = NULL;
    s_fill = NULL;
    s_crest = NULL;
    s_time = NULL;
    s_form = NULL;
    s_over_time = NULL;
    s_over_word = NULL;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grains[i] = NULL;
    for (int i = 0; i < SAND_PRESET_COUNT; i++) {
        s_vial[i].shell = NULL;
        s_vial[i].pile = NULL;
        s_vial[i].num = NULL;
        s_vial[i].unit = NULL;
    }

    lv_obj_t *scr = make_screen();
    lv_obj_t *title = label(scr, SAND_STR_TITLE, &sand_font_16, COL_INK);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 14);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    for (int i = 0; i < SAND_MENU_COUNT; i++) {
        int y = CARD_Y0 + i * (CARD_H + CARD_GAP);
        menu_card_t *card = &s_card[i];
        card->shell = box(scr, CARD_X, y, CARD_W, CARD_H, COL_VIAL, 22);
        /* Rounded clip allocates an ARGB layer. If it does not fit the LVGL
         * pool, the refresh retries the allocation forever and starves idle. */
        card->name = label(card->shell, MENU_NAME[i], &sand_font_16, COL_INK);
        lv_obj_align(card->name, LV_ALIGN_LEFT_MID, 14, 0);
        card->pile = NULL;
        card->tag = NULL;
        card->ring = NULL;
        card->hand_h = NULL;
        card->hand_m = NULL;
        if (i == SAND_KIND_POMODORO) {
            card->ring = ring(card->shell, MARK_X, MARK_Y, MARK_D, COL_MUTED);
            card->hand_h = box(card->ring, MARK_D / 2 - 1, 4, 2, 9, COL_MUTED, 1);
            card->hand_m = box(card->ring, MARK_D / 2, MARK_D / 2 - 1, 8, 2, COL_MUTED, 1);
        } else if (i == SAND_KIND_STOPWATCH) {
            for (int m = 0; m < CARD_MOTE_COUNT; m++) {
                card->mote[m] = box(card->shell, RISE_X[m], RISE_Y[m], RISE_S[m], RISE_S[m],
                                    COL_MUTED, LV_RADIUS_CIRCLE);
            }
        } else if (i == SAND_KIND_FORM) {
            card->ring = ring(card->shell, MARK_X, MARK_Y, MARK_D, COL_MUTED);
        } else {
            card->tag = label(card->shell, SAND_STR_ABC, &sand_font_16, COL_INK);
            lv_obj_align(card->tag, LV_ALIGN_RIGHT_MID, -14, 0);
        }
    }

    s_hint = label(scr, SAND_STR_HINT_MENU, &lv_font_montserrat_14, COL_INK);
    style_hint(s_hint, 228);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 258);
    s_hint_color = label(scr, SAND_STR_HINT_COLOR, &lv_font_montserrat_14, COL_MUTED);
    style_hint(s_hint_color, 228);
    lv_obj_align(s_hint_color, LV_ALIGN_TOP_MID, 0, 276);

    load_screen(scr);
    s_mode = UI_MENU;
}

static void paint_menu(const sand_timer_t *timer)
{
    if (timer->menu == s_last_menu && timer->color == s_last_color) return;
    for (int i = 0; i < SAND_MENU_COUNT; i++) {
        bool selected = i == timer->menu;
        menu_card_t *card = &s_card[i];
        uint32_t body = sand_color_rgb(timer->color, selected ? 1 : 0);
        uint32_t hi = sand_color_rgb(timer->color, selected ? 2 : 0);
        lv_obj_set_style_outline_width(card->shell, selected ? 2 : 0, 0);
        lv_obj_set_style_outline_pad(card->shell, 1, 0);
        lv_obj_set_style_outline_color(card->shell, lv_color_hex(sand_color_rgb(timer->color, 2)), 0);
        lv_obj_set_style_outline_opa(card->shell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(card->name, lv_color_hex(selected ? hi : COL_INK), 0);
        if (card->tag) lv_obj_set_style_text_color(card->tag, lv_color_hex(selected ? hi : body), 0);
        if (card->ring) {
            lv_obj_set_style_border_color(card->ring, lv_color_hex(body), 0);
        }
        if (card->hand_h) lv_obj_set_style_bg_color(card->hand_h, lv_color_hex(selected ? hi : body), 0);
        if (card->hand_m) lv_obj_set_style_bg_color(card->hand_m, lv_color_hex(selected ? hi : body), 0);
        if (card->pile) {
            int pile_h = selected ? 12 : 8;
            int pile_w = CARD_W - 32;
            lv_obj_set_style_bg_color(card->pile, lv_color_hex(body), 0);
            lv_obj_set_size(card->pile, pile_w, pile_h);
            lv_obj_set_pos(card->pile, 16, CARD_H - pile_h - 2);
        }
        for (int m = 0; m < CARD_MOTE_COUNT; m++) {
            if (!card->mote[m]) continue;
            lv_obj_set_style_bg_color(card->mote[m], lv_color_hex(selected ? hi : body), 0);
        }
    }
    s_last_menu = timer->menu;
    s_last_color = timer->color;
}

static void build_pick(void)
{
    reset_cache();
    clear_cards();
    clear_rows();
    s_fill = NULL;
    s_crest = NULL;
    s_time = NULL;
    s_form = NULL;
    s_hint = NULL;
    s_over_time = NULL;
    s_over_word = NULL;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grains[i] = NULL;

    lv_obj_t *scr = make_screen();
    lv_obj_t *title = label(scr, SAND_STR_TITLE, &sand_font_16, COL_INK);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 14);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    int total = VIAL_W * SAND_PRESET_COUNT + VIAL_GAP * (SAND_PRESET_COUNT - 1);
    int x0 = (SAND_SCREEN_W - total) / 2;
    for (int i = 0; i < SAND_PRESET_COUNT; i++) {
        int x = x0 + i * (VIAL_W + VIAL_GAP);
        vial_ui_t *vial = &s_vial[i];
        vial->shell = box(scr, x, VIAL_Y, VIAL_W, VIAL_H, COL_VIAL, 22);
        vial->pile = box(vial->shell, 0, VIAL_H - 32, VIAL_W, 32, COL_MUTED, 22);
        vial->num = label(vial->shell, PRESET_NUM[i], &sand_font_32, COL_INK);
        lv_obj_align(vial->num, LV_ALIGN_TOP_MID, 0, 16);
        vial->unit = label(vial->shell, SAND_STR_MINUTE, &sand_font_16, COL_MUTED);
        lv_obj_align(vial->unit, LV_ALIGN_TOP_MID, 0, 44);
    }

    s_hint = label(scr, SAND_STR_HINT_PICK, &sand_font_16, COL_INK);
    style_hint(s_hint, 228);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 228);

    s_hint_color = label(scr, SAND_STR_HINT_BACK, &sand_font_16, COL_MUTED);
    style_hint(s_hint_color, 228);
    lv_obj_align(s_hint_color, LV_ALIGN_TOP_MID, 0, 258);

    s_swatch = box(scr, 0, 0, 18, 18, sand_color_rgb(0, 1), LV_RADIUS_CIRCLE);
    lv_obj_align(s_swatch, LV_ALIGN_TOP_MID, 0, 286);

    load_screen(scr);
    s_mode = UI_PICK;
}

static void paint_vials(const sand_timer_t *timer)
{
    if (timer->preset == s_last_preset && timer->color == s_last_color) return;
    for (int i = 0; i < SAND_PRESET_COUNT; i++) {
        bool selected = i == timer->preset;
        vial_ui_t *vial = &s_vial[i];
        int pile_h = selected ? 80 : 32;
        uint32_t pile = sand_color_rgb(timer->color, selected ? 1 : 0);
        lv_obj_set_style_bg_color(vial->pile, lv_color_hex(pile), 0);
        lv_obj_set_size(vial->pile, VIAL_W, pile_h);
        lv_obj_set_pos(vial->pile, 0, VIAL_H - pile_h);
        lv_obj_set_style_outline_width(vial->shell, selected ? 2 : 0, 0);
        lv_obj_set_style_outline_pad(vial->shell, 1, 0);
        lv_obj_set_style_outline_color(vial->shell, lv_color_hex(sand_color_rgb(timer->color, 2)), 0);
        lv_obj_set_style_outline_opa(vial->shell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(vial->num,
            lv_color_hex(selected ? sand_color_rgb(timer->color, 2) : COL_INK), 0);
    }
    if (s_swatch) {
        lv_obj_set_style_bg_color(s_swatch, lv_color_hex(sand_color_rgb(timer->color, 1)), 0);
    }
    s_last_preset = timer->preset;
    s_last_color = timer->color;
}

static void clear_vials(void)
{
    for (int i = 0; i < SAND_PRESET_COUNT; i++) {
        s_vial[i].shell = NULL;
        s_vial[i].pile = NULL;
        s_vial[i].num = NULL;
        s_vial[i].unit = NULL;
    }
}

static void build_setup(void)
{
    reset_cache();
    clear_cards();
    clear_rows();
    clear_vials();
    s_fill = NULL;
    s_crest = NULL;
    s_time = NULL;
    s_form = NULL;
    s_over_time = NULL;
    s_over_word = NULL;
    s_swatch = NULL;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grains[i] = NULL;

    lv_obj_t *scr = make_screen();
    lv_obj_t *title = label(scr, SAND_STR_FORM, &sand_font_16, COL_INK);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 14);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    static const char *keys[SAND_ROW_COUNT] = {
        SAND_STR_TIME_ROW, SAND_STR_SHAPE, SAND_STR_MATERIAL, SAND_STR_START
    };
    for (int i = 0; i < SAND_ROW_COUNT; i++) {
        int y = ROW_Y0 + i * (ROW_H + ROW_GAP);
        setup_row_t *row = &s_row[i];
        row->shell = box(scr, ROW_X, y, ROW_W, ROW_H, COL_VIAL, 16);
        row->key = label(row->shell, keys[i], &sand_font_16, COL_INK);
        lv_obj_align(row->key, LV_ALIGN_LEFT_MID, 16, 0);
        row->value = NULL;
        row->swatch = NULL;
        if (i == SAND_ROW_COLOR) {
            row->swatch = box(row->shell, ROW_W - 36, 14, 18, 18, sand_material_rgb(0, 1), LV_RADIUS_CIRCLE);
            row->value = label(row->shell, SAND_STR_FIRE, &sand_font_16, COL_MUTED);
            lv_obj_align(row->value, LV_ALIGN_RIGHT_MID, -44, 0);
        } else if (i != SAND_ROW_START) {
            row->value = label(row->shell, "10 MIN", &sand_font_16, COL_MUTED);
            lv_obj_align(row->value, LV_ALIGN_RIGHT_MID, -16, 0);
        }
    }

    s_hint = label(scr, SAND_STR_HINT_SETUP, &sand_font_16, COL_INK);
    style_hint(s_hint, 228);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 258);
    s_hint_color = label(scr, SAND_STR_HINT_BACK, &sand_font_16, COL_MUTED);
    style_hint(s_hint_color, 228);
    lv_obj_align(s_hint_color, LV_ALIGN_TOP_MID, 0, 282);

    load_screen(scr);
    s_mode = UI_SETUP;
}

static void paint_setup(const sand_timer_t *timer)
{
    char value[16];
    bool changed = timer->setup_row != s_last_row || timer->material != s_last_color
        || timer->form_choice != s_last_choice || timer->form_time != s_last_form_time;
    if (!changed) return;

    snprintf(value, sizeof(value), "%d %s", sand_form_time_min(timer->form_time), SAND_STR_MINUTE);
    lv_label_set_text(s_row[SAND_ROW_TIME].value, value);
    sand_choice_name(timer->form_choice, value, sizeof(value));
    lv_label_set_text(s_row[SAND_ROW_SHAPE].value, value);
    sand_material_name(timer->material, value, sizeof(value));
    if (s_row[SAND_ROW_COLOR].value) lv_label_set_text(s_row[SAND_ROW_COLOR].value, value);
    if (s_row[SAND_ROW_COLOR].swatch) {
        lv_obj_set_style_bg_color(s_row[SAND_ROW_COLOR].swatch,
            lv_color_hex(sand_material_rgb(timer->material, 1)), 0);
    }
    for (int i = 0; i < SAND_ROW_COUNT; i++) {
        bool selected = i == timer->setup_row;
        setup_row_t *row = &s_row[i];
        lv_obj_set_style_outline_width(row->shell, selected ? 2 : 0, 0);
        lv_obj_set_style_outline_pad(row->shell, 1, 0);
        lv_obj_set_style_outline_color(row->shell, lv_color_hex(sand_material_rgb(timer->material, 2)), 0);
        lv_obj_set_style_outline_opa(row->shell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(row->key,
            lv_color_hex(selected ? sand_material_rgb(timer->material, 2) : COL_INK), 0);
        if (row->value) {
            lv_obj_set_style_text_color(row->value,
                lv_color_hex(selected ? sand_material_rgb(timer->material, 1) : COL_MUTED), 0);
        }
    }
    lv_label_set_text(s_hint, timer->setup_row == SAND_ROW_START
        ? SAND_STR_HINT_PICK : SAND_STR_HINT_SETUP);
    s_last_row = timer->setup_row;
    s_last_color = timer->material;
    s_last_choice = timer->form_choice;
    s_last_form_time = timer->form_time;
}

static void build_letter(void)
{
    reset_cache();
    clear_cards();
    clear_rows();
    clear_vials();
    s_fill = NULL;
    s_crest = NULL;
    s_time = NULL;
    s_form = NULL;
    s_over_time = NULL;
    s_over_word = NULL;
    s_swatch = NULL;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grains[i] = NULL;

    lv_obj_t *scr = make_screen();
    lv_obj_t *title = label(scr, SAND_STR_LETTER, &sand_font_16, COL_INK);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 16, 14);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    static const char *keys[SAND_LROW_COUNT] = {
        SAND_STR_LETTER, SAND_STR_TIME_ROW, SAND_STR_COLOR, SAND_STR_START
    };
    for (int i = 0; i < SAND_LROW_COUNT; i++) {
        int y = 46 + i * 42;
        setup_row_t *row = &s_row[i];
        row->shell = box(scr, ROW_X, y, ROW_W, 38, COL_VIAL, 14);
        row->key = label(row->shell, keys[i], &sand_font_16, COL_INK);
        lv_obj_align(row->key, LV_ALIGN_LEFT_MID, 14, 0);
        row->value = NULL;
        row->swatch = NULL;
        if (i == SAND_LROW_COLOR) {
            row->swatch = box(row->shell, ROW_W - 34, 10, 18, 18, sand_color_rgb(0, 1), LV_RADIUS_CIRCLE);
        } else if (i != SAND_LROW_START) {
            row->value = label(row->shell, "A", &sand_font_16, COL_MUTED);
            lv_obj_align(row->value, LV_ALIGN_RIGHT_MID, -14, 0);
        }
    }

    s_hint = label(scr, SAND_STR_HINT_SETUP, &sand_font_16, COL_INK);
    style_hint(s_hint, 228);
    lv_obj_align(s_hint, LV_ALIGN_TOP_MID, 0, 262);
    s_hint_color = label(scr, SAND_STR_HINT_BACK, &sand_font_16, COL_MUTED);
    style_hint(s_hint_color, 228);
    lv_obj_align(s_hint_color, LV_ALIGN_TOP_MID, 0, 286);

    load_screen(scr);
    s_mode = UI_LETTER;
}

static void paint_letter(const sand_timer_t *timer)
{
    char value[20];
    int letter = timer->letter;
    if (letter < 0 || letter > 25) letter = 0;
    snprintf(value, sizeof(value), "%c", 'A' + letter);
    lv_label_set_text(s_row[SAND_LROW_GLYPH].value, value);
    snprintf(value, sizeof(value), "%d %s", sand_form_time_min(timer->form_time), SAND_STR_MINUTE);
    lv_label_set_text(s_row[SAND_LROW_TIME].value, value);
    if (s_row[SAND_LROW_COLOR].swatch) {
        lv_obj_set_style_bg_color(s_row[SAND_LROW_COLOR].swatch,
            lv_color_hex(sand_color_rgb(timer->color, 1)), 0);
    }
    for (int i = 0; i < SAND_LROW_COUNT; i++) {
        bool selected = i == timer->setup_row;
        setup_row_t *row = &s_row[i];
        lv_obj_set_style_outline_width(row->shell, selected ? 2 : 0, 0);
        lv_obj_set_style_outline_pad(row->shell, 1, 0);
        lv_obj_set_style_outline_color(row->shell, lv_color_hex(sand_color_rgb(timer->color, 2)), 0);
        lv_obj_set_style_outline_opa(row->shell, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(row->key,
            lv_color_hex(selected ? sand_color_rgb(timer->color, 2) : COL_INK), 0);
        if (row->value) {
            lv_obj_set_style_text_color(row->value,
                lv_color_hex(selected ? sand_color_rgb(timer->color, 1) : COL_MUTED), 0);
        }
    }
    lv_label_set_text(s_hint, timer->setup_row == SAND_LROW_START
        ? SAND_STR_HINT_PICK : SAND_STR_HINT_SETUP);
    lv_label_set_text(s_hint_color, SAND_STR_HINT_BACK);
}

static void build_fall(const sand_timer_t *timer)
{
    reset_cache();
    clear_cards();
    clear_rows();
    clear_vials();
    s_swatch = NULL;
    s_hint = NULL;
    s_hint_color = NULL;
    s_form = NULL;

    lv_obj_t *scr = make_screen();
    uint32_t body = sand_color_rgb(timer->color, 1);
    s_fill = box(scr, 0, SAND_SCREEN_H, SAND_SCREEN_W, 1, body, 0);
    s_crest = box(scr, 0, SAND_SCREEN_H, SAND_SCREEN_W, 1, sand_color_rgb(timer->color, 2), 0);
    lv_obj_add_flag(s_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_crest, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        s_grains[i] = box(scr, 12, 48, 5, 5, body, LV_RADIUS_CIRCLE);
        lv_obj_add_flag(s_grains[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_time = label(scr, "00:00", &sand_font_16, COL_INK);
    style_readout(s_time);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 12, 10);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    s_over_time = label(scr, SAND_STR_TIME, &sand_font_64, COL_BG);
    s_over_word = label(scr, SAND_STR_OVER, &sand_font_64, COL_BG);
    lv_obj_align(s_over_time, LV_ALIGN_CENTER, 0, -30);
    lv_obj_align(s_over_word, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_over_word, LV_OBJ_FLAG_HIDDEN);

    load_screen(scr);
    s_mode = UI_FALL;
}

static void build_form(const sand_timer_t *timer)
{
    reset_cache();
    clear_cards();
    clear_rows();
    clear_vials();
    s_swatch = NULL;
    s_hint = NULL;
    s_hint_color = NULL;
    s_fill = NULL;
    s_crest = NULL;

    lv_obj_t *scr = make_screen();
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grains[i] = NULL;
    if (!s_form_ready) {
        LV_DRAW_BUF_INIT_STATIC(form_dots);
        LV_DRAW_BUF_INIT_STATIC(settled_dots);
        s_form_ready = true;
    }
    s_canvas = lv_canvas_create(scr);
    lv_canvas_set_draw_buf(s_canvas, &form_dots);
    lv_obj_set_pos(s_canvas, 0, 0);
    lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);

    s_form = label(scr, SAND_STR_FORM, &sand_font_16, COL_INK);
    style_readout(s_form);
    lv_obj_align(s_form, LV_ALIGN_TOP_LEFT, 12, 8);
    s_time = label(scr, "00:00", &sand_font_16, COL_INK);
    style_readout(s_time);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 12, 34);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    s_over_time = label(scr, "00:00", &sand_font_32, COL_INK);
    lv_obj_align(s_over_time, LV_ALIGN_CENTER, 0, 0);
    s_over_word = label(scr, SAND_STR_OVER, &sand_font_64, COL_BG);
    lv_obj_add_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_over_word, LV_OBJ_FLAG_HIDDEN);

    load_screen(scr);
    s_mode = UI_FORM;
}

static void paint_clock(const sand_timer_t *timer);
static void paint_grains(const sand_timer_t *timer);

static void build_rise(const sand_timer_t *timer)
{
    reset_cache();
    clear_cards();
    clear_rows();
    clear_vials();
    s_swatch = NULL;
    s_hint = NULL;
    s_hint_color = NULL;
    s_fill = NULL;
    s_crest = NULL;
    s_form = NULL;
    s_over_time = NULL;
    s_over_word = NULL;
    for (int i = 0; i < SAND_PRESET_COUNT; i++) {
        s_vial[i].shell = NULL;
        s_vial[i].pile = NULL;
        s_vial[i].num = NULL;
        s_vial[i].unit = NULL;
    }

    lv_obj_t *scr = make_screen();
    uint32_t body = sand_color_rgb(timer->color, 1);
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        s_grains[i] = box(scr, 0, SAND_SCREEN_H + 8, 4, 4, body, LV_RADIUS_CIRCLE);
        lv_obj_add_flag(s_grains[i], LV_OBJ_FLAG_HIDDEN);
    }

    s_time = label(scr, "00:00", &sand_font_16, COL_INK);
    style_readout(s_time);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 12, 10);
    s_bat_pct = label(scr, "0%", &sand_font_16, COL_INK);
    style_readout(s_bat_pct);
    lv_obj_align(s_bat_pct, LV_ALIGN_TOP_RIGHT, -12, 10);

    load_screen(scr);
    s_mode = UI_RISE;
}

static void paint_rise(const sand_timer_t *timer)
{
    if (timer->color != s_last_color) {
        s_last_color = timer->color;
        for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grain_shade[i] = -1;
    }
    paint_clock(timer);
    paint_grains(timer);
}

static void paint_form(const sand_timer_t *timer)
{
    char name[16];
    sand_form_name(timer, name, sizeof(name));
    if (strcmp(name, s_last_form) != 0) {
        lv_label_set_text(s_form, name);
        snprintf(s_last_form, sizeof(s_last_form), "%s", name);
    }
}

static void paint_over(const sand_timer_t *timer)
{
    /* Form and Letter keep the finished picture. Only Pomodoro shows Time Over. */
    bool done = timer->phase == SAND_PHASE_DONE
        && timer->kind != SAND_KIND_FORM
        && timer->kind != SAND_KIND_LETTER;
    if (done == s_over_shown) return;
    if (done) {
        lv_obj_remove_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_over_word, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_over_word, LV_OBJ_FLAG_HIDDEN);
    }
    s_over_shown = done;
}

static void paint_pile(const sand_timer_t *timer, bool color_changed)
{
    int fill = sand_fill_px(timer->elapsed_ms, timer->duration_ms, SAND_SCREEN_H);
    if (!color_changed && fill == s_last_fill) return;
    if (fill <= 0) {
        lv_obj_add_flag(s_fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_crest, LV_OBJ_FLAG_HIDDEN);
    } else {
        int crest_h = fill > 10 ? 8 : fill;
        lv_obj_remove_flag(s_fill, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_crest, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_fill, lv_color_hex(sand_color_rgb(timer->color, 1)), 0);
        lv_obj_set_style_bg_color(s_crest, lv_color_hex(sand_color_rgb(timer->color, 2)), 0);
        lv_obj_set_size(s_fill, SAND_SCREEN_W, fill);
        lv_obj_set_pos(s_fill, 0, SAND_SCREEN_H - fill);
        lv_obj_set_size(s_crest, SAND_SCREEN_W, crest_h);
        lv_obj_set_pos(s_crest, 0, SAND_SCREEN_H - fill);
    }
    s_last_fill = fill;
}

static void paint_fall(const sand_timer_t *timer)
{
    bool color_changed = timer->color != s_last_color;
    if (color_changed) {
        s_last_color = timer->color;
        for (int i = 0; i < SAND_GRAIN_COUNT; i++) s_grain_shade[i] = -1;
    }
    paint_pile(timer, color_changed);
    paint_clock(timer);
    paint_over(timer);
    paint_grains(timer);
}

static lv_color32_t rgb32(uint32_t rgb)
{
    lv_color32_t color = {
        .blue = (uint8_t)(rgb & 255u),
        .green = (uint8_t)((rgb >> 8) & 255u),
        .red = (uint8_t)((rgb >> 16) & 255u),
        .alpha = 255,
    };
    return color;
}

static void plot_px(lv_draw_buf_t *buf, int px, int py, int index)
{
    uint8_t *data;
    uint8_t shift;
    if (px < 0 || py < 0 || px >= SAND_SCREEN_W || py >= SAND_SCREEN_H) return;
    if (index < 0) index = 0;
    if (index > 15) index = 15;
    data = lv_draw_buf_goto_xy(buf, (uint32_t)px, (uint32_t)py);
    if (!data) return;
    shift = (uint8_t)(4 - 4 * (px & 1));
    *data = (uint8_t)((*data & ~(0x0Fu << shift)) | ((unsigned)index << shift));
}

static const int8_t ROT_C[8] = {16, 11, 0, -11, -16, -11, 0, 11};
static const int8_t ROT_S[8] = {0, 11, 16, 11, 0, -11, -16, -11};

static int sprite_on(int kind, int shape, int col, int row, int w, int h)
{
    if (col < 0 || row < 0 || col >= w || row >= h) return 0;
    if (kind == SAND_KIND_LETTER) return sand_bold_pixel(shape, col * 2, row * 2);
    if (shape == SAND_CHOICE_SQUARE || shape == SAND_CHOICE_RECT) return 1;
    if (shape == SAND_CHOICE_TRIANGLE) {
        int half = ((row + 1) * (w / 2)) / (h > 0 ? h : 1);
        int mid = w / 2;
        return col >= mid - half && col <= mid + half;
    }
    if (shape == SAND_CHOICE_DIAMOND) {
        int mid_row = h / 2;
        int dist;
        int half;
        if (mid_row < 1) mid_row = 1;
        dist = row < mid_row ? row : (h - 1 - row);
        half = 1 + (dist * (w / 2)) / mid_row;
        return col >= w / 2 - half && col <= w / 2 + half;
    }
    if (shape == SAND_CHOICE_HEXAGON) {
        int cap = h / 4;
        int inset = 0;
        if (cap < 1) cap = 1;
        if (row < cap) inset = ((cap - row) * (w / 4)) / cap;
        else if (row >= h - cap) inset = ((row - (h - cap) + 1) * (w / 4)) / cap;
        return col >= inset && col <= w - 1 - inset;
    }
    {
        int dx = col - w / 2;
        int dy = row - h / 2;
        int rx = w / 2;
        int ry = h / 2;
        if (rx < 1) rx = 1;
        if (ry < 1) ry = 1;
        return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
    }
}

static void plot_rotated(lv_draw_buf_t *buf, int kind, int shape, int spin,
                         int x, int y, int box_w, int box_h, int index)
{
    int sprite_w = 10;
    int sprite_h = 10;
    int turn = spin & 7;
    int cosine = ROT_C[turn];
    int sine = ROT_S[turn];
    int cx;
    int cy;
    sand_sprite_size(kind, shape, &sprite_w, &sprite_h);
    cx = x + box_w / 2;
    cy = y + box_h / 2;
    for (int row = 0; row < sprite_h; row++) {
        for (int col = 0; col < sprite_w; col++) {
            int lx;
            int ly;
            int rx;
            int ry;
            if (!sprite_on(kind, shape, col, row, sprite_w, sprite_h)) continue;
            lx = col - sprite_w / 2;
            ly = row - sprite_h / 2;
            rx = (lx * cosine - ly * sine) / 16;
            ry = (lx * sine + ly * cosine) / 16;
            plot_px(buf, cx + rx, cy + ry, index);
        }
    }
}

static int grain_ink(const sand_timer_t *timer, const sand_grain_t *grain)
{
    int shade = grain->shade;
    if (shade < 0) shade = 0;
    if (shade > 2) shade = 2;
    if (timer->kind == SAND_KIND_LETTER) {
        int color = grain->lz;
        if (color < 0 || color >= SAND_COLOR_COUNT) color = 0;
        return color * 2 + (shade > 0 ? 1 : 0) + 1;
    }
    {
        int material = grain->lz;
        if (material < 0 || material >= SAND_MATERIAL_COUNT) material = 0;
        return material * 3 + shade + 1;
    }
}

static uint8_t s_stamp[SAND_PILE_COUNT];
static int16_t s_air_x[SAND_PILE_COUNT];
static int16_t s_air_y[SAND_PILE_COUNT];
static uint8_t s_air_on[SAND_PILE_COUNT];
static int s_stamped;
static int s_layer_reset;

#define SAND_DIRTY_MAX 24

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} sand_rect_t;

static void copy_i4_rect(lv_draw_buf_t *dst, lv_draw_buf_t *src, int x, int y, int w, int h)
{
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    int bx0;
    int bx1;
    int bytes;
    if (w < 1 || h < 1) return;
    if (x1 > SAND_SCREEN_W) x1 = SAND_SCREEN_W;
    if (y1 > SAND_SCREEN_H) y1 = SAND_SCREEN_H;
    if (x0 >= x1 || y0 >= y1) return;
    bx0 = x0 & ~1;
    bx1 = (x1 + 1) & ~1;
    if (bx1 > SAND_SCREEN_W) bx1 = SAND_SCREEN_W;
    bytes = (bx1 - bx0) / 2;
    if (bytes < 1) return;
    for (int row = y0; row < y1; row++) {
        uint8_t *from = lv_draw_buf_goto_xy(src, (uint32_t)bx0, (uint32_t)row);
        uint8_t *to = lv_draw_buf_goto_xy(dst, (uint32_t)bx0, (uint32_t)row);
        if (from && to) memcpy(to, from, (size_t)bytes);
    }
}

static void remember_rect(sand_rect_t *rect, int *count, int *spill,
                          int *x0, int *y0, int *x1, int *y1,
                          int x, int y, int w, int h)
{
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    if (x < *x0) *x0 = x;
    if (y < *y0) *y0 = y;
    if (x + w > *x1) *x1 = x + w;
    if (y + h > *y1) *y1 = y + h;
    if (*spill || *count >= SAND_DIRTY_MAX) {
        *spill = 1;
        return;
    }
    rect[*count].x = (int16_t)x;
    rect[*count].y = (int16_t)y;
    rect[*count].w = (int16_t)w;
    rect[*count].h = (int16_t)h;
    (*count)++;
}

static void invalidate_local(int x, int y, int w, int h)
{
    lv_area_t coords;
    lv_area_t area;
    if (w < 1 || h < 1) return;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > SAND_SCREEN_W) w = SAND_SCREEN_W - x;
    if (y + h > SAND_SCREEN_H) h = SAND_SCREEN_H - y;
    if (w < 1 || h < 1) return;
    lv_obj_get_coords(s_canvas, &coords);
    area.x1 = coords.x1 + x;
    area.y1 = coords.y1 + y;
    area.x2 = coords.x1 + x + w - 1;
    area.y2 = coords.y1 + y + h - 1;
    lv_obj_invalidate_area(s_canvas, &area);
}

static void clear_settled(void)
{
    uint8_t *pixels = lv_draw_buf_goto_xy(&settled_dots, 0, 0);
    if (pixels) {
        memset(pixels, 0, (size_t)settled_dots.header.stride * (size_t)settled_dots.header.h);
    }
    memset(s_stamp, 0, sizeof(s_stamp));
    memset(s_air_on, 0, sizeof(s_air_on));
    s_stamped = 0;
    s_layer_reset = 1;
}

static void paint_stipple(const sand_timer_t *timer)
{
    lv_draw_buf_t *buf;
    uint8_t *pixels;
    uint8_t *settled;
    sand_rect_t dirty[SAND_DIRTY_MAX];
    int dirty_n = 0;
    int spill = 0;
    int box_x0 = SAND_SCREEN_W;
    int box_y0 = SAND_SCREEN_H;
    int box_x1 = 0;
    int box_y1 = 0;
    int marked = 0;
    int fresh = 0;
    int full = 0;
    if (!s_canvas) return;
    buf = lv_canvas_get_draw_buf(s_canvas);
    if (!buf || !buf->data) return;
    {
        int palette = timer->kind == SAND_KIND_LETTER ? 2 : 1;
        if (palette != s_canvas_color) {
            lv_canvas_set_palette(s_canvas, 0, rgb32(COL_BG));
            if (timer->kind == SAND_KIND_LETTER) {
                for (int color = 0; color < SAND_COLOR_COUNT; color++) {
                    for (int band = 0; band < 2; band++) {
                        int index = color * 2 + band + 1;
                        lv_canvas_set_palette(s_canvas, (uint32_t)index,
                            rgb32(sand_color_rgb(color, band == 0 ? 0 : 2)));
                    }
                }
            } else {
                for (int material = 0; material < SAND_MATERIAL_COUNT; material++) {
                    for (int shade = 0; shade < 3; shade++) {
                        int index = material * 3 + shade + 1;
                        lv_canvas_set_palette(s_canvas, (uint32_t)index,
                            rgb32(sand_material_rgb(material, shade)));
                    }
                }
            }
            s_canvas_color = palette;
            clear_settled();
        }
    }
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        if (timer->grains[i].alive && timer->grains[i].mark) marked++;
    }
    if (marked < s_stamped) clear_settled();
    if (s_layer_reset) {
        full = 1;
        fresh = 1;
        s_layer_reset = 0;
    }
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        const sand_grain_t *grain = &timer->grains[i];
        int height;
        if (!grain->alive || !grain->mark || s_stamp[i]) continue;
        height = grain->ly > 0 ? grain->ly : grain->size;
        if (s_air_on[i]) {
            remember_rect(dirty, &dirty_n, &spill, &box_x0, &box_y0, &box_x1, &box_y1,
                          s_air_x[i] - 4, s_air_y[i] - 4, grain->size + 8, height + 8);
            s_air_on[i] = 0;
        }
        plot_rotated(&settled_dots, timer->kind, grain->tone, grain->spin,
                     grain->x, grain->y, grain->size, height, grain_ink(timer, grain));
        s_stamp[i] = 1;
        s_stamped++;
        fresh = 1;
        remember_rect(dirty, &dirty_n, &spill, &box_x0, &box_y0, &box_x1, &box_y1,
                      grain->x - 4, grain->y - 4, grain->size + 8, height + 8);
    }
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        const sand_grain_t *grain = &timer->grains[i];
        int height;
        if (!grain->alive || grain->mark) {
            s_air_on[i] = 0;
            continue;
        }
        height = grain->ly > 0 ? grain->ly : grain->size;
        if (grain->y + height <= 0 && !s_air_on[i]) continue;
        if (!s_air_on[i] || s_air_x[i] != grain->x || s_air_y[i] != grain->y) {
            if (s_air_on[i]) {
                remember_rect(dirty, &dirty_n, &spill, &box_x0, &box_y0, &box_x1, &box_y1,
                              s_air_x[i] - 4, s_air_y[i] - 4, grain->size + 8, height + 8);
            }
            remember_rect(dirty, &dirty_n, &spill, &box_x0, &box_y0, &box_x1, &box_y1,
                          grain->x - 4, grain->y - 4, grain->size + 8, height + 8);
            fresh = 1;
        }
        s_air_x[i] = (int16_t)grain->x;
        s_air_y[i] = (int16_t)grain->y;
        s_air_on[i] = 1;
    }
    if (!fresh) return;
    if (full) {
        pixels = lv_draw_buf_goto_xy(buf, 0, 0);
        settled = lv_draw_buf_goto_xy(&settled_dots, 0, 0);
        if (pixels && settled) {
            memcpy(pixels, settled, (size_t)buf->header.stride * (size_t)buf->header.h);
        }
    } else if (spill) {
        copy_i4_rect(buf, &settled_dots, box_x0, box_y0, box_x1 - box_x0, box_y1 - box_y0);
    } else {
        for (int i = 0; i < dirty_n; i++) {
            copy_i4_rect(buf, &settled_dots, dirty[i].x, dirty[i].y, dirty[i].w, dirty[i].h);
        }
    }
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        const sand_grain_t *grain = &timer->grains[i];
        int height;
        if (!grain->alive || grain->mark) continue;
        height = grain->ly > 0 ? grain->ly : grain->size;
        if (grain->y + height <= 0) continue;
        plot_rotated(buf, timer->kind, grain->tone, grain->spin,
                     grain->x, grain->y, grain->size, height, grain_ink(timer, grain));
    }
    lv_image_cache_drop(buf);
    if (full) {
        lv_obj_invalidate(s_canvas);
    } else if (spill) {
        invalidate_local(box_x0, box_y0, box_x1 - box_x0, box_y1 - box_y0);
    } else {
        for (int i = 0; i < dirty_n; i++) {
            invalidate_local(dirty[i].x, dirty[i].y, dirty[i].w, dirty[i].h);
        }
    }
}

static void paint_center_clock(const sand_timer_t *timer)
{
    bool show = timer->phase == SAND_PHASE_DONE
        && (timer->kind == SAND_KIND_FORM || timer->kind == SAND_KIND_LETTER);
    if (!s_over_time) return;
    if (!show) {
        if (timer->kind == SAND_KIND_FORM || timer->kind == SAND_KIND_LETTER) {
            if (!lv_obj_has_flag(s_over_time, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_time && lv_obj_has_flag(s_time, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_remove_flag(s_time, LV_OBJ_FLAG_HIDDEN);
            }
        }
        return;
    }
    lv_label_set_text(s_over_time, s_last_clock);
    lv_obj_align(s_over_time, LV_ALIGN_CENTER, 0, 0);
    if (lv_obj_has_flag(s_over_time, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_remove_flag(s_over_time, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_over_word && !lv_obj_has_flag(s_over_word, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(s_over_word, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_time && !lv_obj_has_flag(s_time, LV_OBJ_FLAG_HIDDEN)) {
        lv_obj_add_flag(s_time, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_move_foreground(s_over_time);
}

static void paint_gather(const sand_timer_t *timer)
{
    if (timer->color != s_last_color) s_last_color = timer->color;
    paint_stipple(timer);
    paint_form(timer);
    paint_clock(timer);
    paint_center_clock(timer);
}

static void paint_clock(const sand_timer_t *timer)
{
    char clock[8];
    sand_format_clock(timer, clock, sizeof(clock));
    if (strcmp(clock, s_last_clock) != 0) {
        lv_label_set_text(s_time, clock);
        memcpy(s_last_clock, clock, sizeof(s_last_clock));
    }
}

static void paint_grains(const sand_timer_t *timer)
{
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        const sand_grain_t *grain = &timer->grains[i];
        lv_obj_t *obj = s_grains[i];
        if (!grain->alive) {
            if (!lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
            }
            s_grain_x[i] = -10000;
            continue;
        }
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
        if (grain->size != s_grain_size[i]) {
            lv_obj_set_size(obj, grain->size, grain->size);
            s_grain_size[i] = grain->size;
        }
        if (grain->shade != s_grain_shade[i]) {
            lv_obj_set_style_bg_color(obj, lv_color_hex(sand_color_rgb(timer->color, grain->shade)), 0);
            s_grain_shade[i] = grain->shade;
        }
        if (grain->x != s_grain_x[i] || grain->y != s_grain_y[i]) {
            lv_obj_set_pos(obj, grain->x, grain->y);
            s_grain_x[i] = grain->x;
            s_grain_y[i] = grain->y;
        }
    }
}

void sand_ui_sync(const sand_timer_t *timer, int battery_soc)
{
    int want = UI_FALL;
    if (timer->phase == SAND_PHASE_MENU) want = UI_MENU;
    else if (timer->phase == SAND_PHASE_PICK) want = UI_PICK;
    else if (timer->phase == SAND_PHASE_SETUP && timer->kind == SAND_KIND_LETTER) want = UI_LETTER;
    else if (timer->phase == SAND_PHASE_SETUP) want = UI_SETUP;
    else if (timer->kind == SAND_KIND_STOPWATCH) want = UI_RISE;
    else if (timer->kind == SAND_KIND_FORM || timer->kind == SAND_KIND_LETTER) want = UI_FORM;

    if (want != s_mode) {
        if (want == UI_MENU) build_menu();
        else if (want == UI_PICK) build_pick();
        else if (want == UI_SETUP) build_setup();
        else if (want == UI_LETTER) build_letter();
        else if (want == UI_RISE) build_rise(timer);
        else if (want == UI_FORM) build_form(timer);
        else build_fall(timer);
    }
    if (s_mode == UI_MENU) paint_menu(timer);
    else if (s_mode == UI_PICK) paint_vials(timer);
    else if (s_mode == UI_SETUP) paint_setup(timer);
    else if (s_mode == UI_LETTER) paint_letter(timer);
    else if (s_mode == UI_RISE) paint_rise(timer);
    else if (s_mode == UI_FORM) paint_gather(timer);
    else paint_fall(timer);
    sync_battery(battery_soc);
}

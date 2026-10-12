#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SAND_SCREEN_W 240
#define SAND_SCREEN_H 320
#define SAND_MENU_COUNT 4
#define SAND_TEXT_MAX 8
#define SAND_TYPE_COUNT 37
#define SAND_PRESET_COUNT 3
#define SAND_FORM_TIME_COUNT 6
#define SAND_COLOR_COUNT 6
#define SAND_MATERIAL_COUNT 5
#define SAND_GRAIN_COUNT 32
/* Form and Letter pile this many small pieces. Pomodoro and Stopwatch use the first 32. */
#define SAND_PILE_COUNT 436
#define SAND_FORM_DOTS 436

typedef enum {
    SAND_KIND_FORM = 0,
    SAND_KIND_LETTER,
    SAND_KIND_POMODORO,
    SAND_KIND_STOPWATCH,
} sand_kind_t;

typedef enum {
    SAND_PHASE_MENU = 0,
    SAND_PHASE_PICK,
    SAND_PHASE_SETUP,
    SAND_PHASE_RUNNING,
    SAND_PHASE_PAUSED,
    SAND_PHASE_DONE,
} sand_phase_t;

typedef enum {
    SAND_CHOICE_CIRCLE = 0,
    SAND_CHOICE_SQUARE,
    SAND_CHOICE_TRIANGLE,
    SAND_CHOICE_DIAMOND,
    SAND_CHOICE_HEXAGON,
    SAND_CHOICE_RECT,
    SAND_CHOICE_COUNT
} sand_choice_t;

typedef enum {
    SAND_MAT_FIRE = 0,
    SAND_MAT_WATER,
    SAND_MAT_STONE,
    SAND_MAT_YUAN,
    SAND_MAT_WOOD,
    SAND_MAT_COUNT
} sand_material_t;

typedef enum {
    SAND_LROW_GLYPH = 0,
    SAND_LROW_TIME,
    SAND_LROW_COLOR,
    SAND_LROW_START,
    SAND_LROW_COUNT
} sand_lrow_t;

typedef enum {
    SAND_ROW_TIME = 0,
    SAND_ROW_SHAPE,
    SAND_ROW_COLOR,
    SAND_ROW_START,
    SAND_ROW_COUNT
} sand_row_t;

typedef enum {
    SAND_FORM_CIRCLE = 0,
    SAND_FORM_SQUARE,
    SAND_FORM_TRIANGLE,
    SAND_FORM_DIAMOND,
    SAND_FORM_HEXAGON,
    SAND_FORM_RECT,
    SAND_FORM_LETTER,
    SAND_FORM_COUNT
} sand_form_t;

typedef enum {
    SAND_INPUT_UP = 0,
    SAND_INPUT_DOWN,
    SAND_INPUT_OK,
    SAND_INPUT_OK_LONG,
    SAND_INPUT_UP_LONG,
} sand_input_t;

typedef struct {
    int x;
    int y;
    int start_x;
    int start_y;
    int size;
    int speed_px_per_s;
    int shade;
    bool alive;
    /* Fixed stipple sample. Screen position is this point after the slow turn. */
    int16_t lx;
    int16_t ly;
    int16_t lz;
    int8_t mark;
    /* Shape or letter captured when this piece is released. */
    int8_t tone;
    /* Random orientation, in 45-degree steps. */
    int8_t spin;
    /* Horizontal sub-pixel remainder, in thousandths of a pixel. */
    int16_t fx;
} sand_grain_t;

typedef struct {
    int menu;
    sand_kind_t kind;
    int preset;
    int color;
    int material;
    sand_phase_t phase;
    int form_time;
    int form_choice;
    int setup_row;
    bool typing;
    int type_index;
    int custom_len;
    char custom[SAND_TEXT_MAX + 1];
    sand_form_t form;
    int letter;
    int form_ox;
    int form_oy;
    int form_yaw;
    int form_scale;
    uint32_t entropy;
    uint32_t elapsed_ms;
    uint32_t duration_ms;
    uint32_t overtime_ms;
    sand_grain_t grains[SAND_FORM_DOTS];
    uint32_t rng;
} sand_timer_t;

void sand_timer_init(sand_timer_t *timer);
/* Mixed into the next Form start so each run can look different. */
void sand_timer_set_entropy(sand_timer_t *timer, uint32_t entropy);
/* English name of the running piece, such as "CIRCLE" or "LETTER A". */
void sand_form_name(const sand_timer_t *timer, char *buf, size_t len);
/* English name of a setup shape, such as "Circle" or "Rectangle". */
void sand_choice_name(int choice, char *buf, size_t len);
/* English name of a Form material, such as "Fire" or "Wood". */
void sand_material_name(int material, char *buf, size_t len);
uint32_t sand_material_rgb(int material, int shade);
/* Bold 16x22 letter. Column 0 is the left edge. */
int sand_bold_pixel(int glyph, int col, int row);
/* Unrotated sprite size for a Form shape or a Letter glyph. */
void sand_sprite_size(int kind, int shape, int *w, int *h);
/* Typing reel: A-Z, 0-9, then '<' to delete. */
char sand_type_char(int index);
/* Screen point the grain settles on. spin_ms rotates the sphere. */
void sand_form_target(const sand_timer_t *timer, int index, uint32_t spin_ms,
                      int *x, int *y, int *size, int *shade);
uint32_t sand_preset_duration_ms(int preset);
int sand_form_time_min(int index);
uint32_t sand_form_time_ms(int index);
int sand_fill_px(uint32_t elapsed_ms, uint32_t duration_ms, int screen_h);
uint32_t sand_remaining_ms(const sand_timer_t *timer);
void sand_format_remaining(const sand_timer_t *timer, char *buf, size_t len);
/* Pomodoro and Form count down, then up after they finish.
 * Stopwatch counts up from 00:00 for the whole run. */
void sand_format_clock(const sand_timer_t *timer, char *buf, size_t len);
/* Writes "0%".."100%". A negative reading clears the buffer. */
void sand_format_soc(int soc, char *buf, size_t len);
uint32_t sand_color_rgb(int color, int shade);
void sand_timer_on_input(sand_timer_t *timer, sand_input_t input);
void sand_timer_tick(sand_timer_t *timer, uint32_t dt_ms);

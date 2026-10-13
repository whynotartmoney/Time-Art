#include "sand_timer.h"

#include "sand_copy.h"

#include <stdio.h>
#include <string.h>

/* Pomodoro grains fall and pile up. Form and Letter pieces are small, fall
 * with gravity from above the screen, and stack. They are released across
 * the countdown so the pile reaches the top as the timer ends. Each piece
 * keeps the shape, color, and angle it had when it was released.
 * sand_fill_px is only the Pomodoro pile height. */
#define RISE_CLEAR_Y 46

static void sky_reset(void);

static const uint32_t PRESET_MS[SAND_PRESET_COUNT] = {
    10u * 60u * 1000u,
    20u * 60u * 1000u,
    30u * 60u * 1000u,
};

static const int FORM_MIN[SAND_FORM_TIME_COUNT] = {1, 5, 10, 20, 30, 60};

/* shade 0 deep, 1 body, 2 highlight */
static const uint32_t COLOR_RGB[SAND_COLOR_COUNT][3] = {
    {0xC4A04A, 0xE6C36A, 0xF3D98A},
    {0xB85A42, 0xE07A5F, 0xF0A090},
    {0x3E9A6E, 0x6FCB9F, 0xA6E4C4},
    {0x2E7EAE, 0x5EB0E5, 0xA3D4F2},
    {0x7C5CC4, 0xB794F6, 0xD4C2FA},
    {0xCDB892, 0xF3E6C8, 0xFFF8EC},
};

/* Fire, water, stone, yuan, wood. shade 0 deep, 1 body, 2 highlight. */
static const uint32_t MATERIAL_RGB[SAND_MATERIAL_COUNT][3] = {
    {0x9A3412, 0xF97316, 0xFDBA74},
    {0x1E3A8A, 0x38BDF8, 0xBAE6FD},
    {0x44403C, 0x78716C, 0xD6D3D1},
    {0x854D0E, 0xEAB308, 0xFDE68A},
    {0x365314, 0x4D7C0F, 0xA3E635},
};

static uint32_t rng_next(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0x6D2B79F5u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int rng_span(uint32_t *state, int lo, int hi)
{
    int span = hi - lo + 1;
    if (span <= 1) return lo;
    return lo + (int)(rng_next(state) % (uint32_t)span);
}

/* Quarter sine, sin(k * 90deg / 64) * 1024, k = 0..64. */

/* Bold 16x22 letters. Bit 15 is the left column. Drawn at half size. */
static const uint16_t FONT_BOLD[36][22] = {
    {0x01C0, 0x03E0, 0x03E0, 0x03E0, 0x07F0, 0x07F0, 0x07F0, 0x07F0, 0x0FF8, 0x0FF8, 0x0F78, 0x1FFC, 0x1FFC, 0x1FFC, 0x3FFE, 0x3E3E, 0x3C1E, 0x3C1E, 0x7C1F, 0x780F, 0x780F, 0x780F}, /* A */
    {0x1800, 0x3C00, 0x3F80, 0x3FE0, 0x3FF0, 0x3C78, 0x3C38, 0x3C38, 0x3FF8, 0x3FF0, 0x3FE0, 0x3FE0, 0x3FF0, 0x3FF8, 0x3C78, 0x3C3C, 0x3C3C, 0x3E78, 0x3FF8, 0x3FF0, 0x3FC0, 0x3C00}, /* B */
    {0x0000, 0x0000, 0x0000, 0x01E0, 0x07F8, 0x0FF8, 0x0F3C, 0x1E1C, 0x1E00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1C00, 0x1E00, 0x1E1C, 0x0F3C, 0x0FF8, 0x07F8, 0x01E0, 0x0000, 0x0000}, /* C */
    {0x3800, 0x3C00, 0x3C00, 0x3FE0, 0x3FF0, 0x3FF8, 0x3E78, 0x3C3C, 0x3C1C, 0x3C1C, 0x3C1C, 0x3C1C, 0x3C1C, 0x3C1C, 0x3C1C, 0x3C3C, 0x3E78, 0x3FF8, 0x3FF0, 0x3FE0, 0x3C00, 0x3C00}, /* D */
    {0x3800, 0x3FFF, 0x7FFF, 0x7FFF, 0x3FFE, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3FF8, 0x3FFC, 0x7FFC, 0x3FFC, 0x3FF8, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3FFE, 0x7FFF, 0x7FFF, 0x3FFF}, /* E */
    {0x3800, 0x3FFF, 0x7FFF, 0x7FFF, 0x3FFE, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3FF8, 0x3FFC, 0x7FFC, 0x3FFC, 0x3FF8, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00}, /* F */
    {0x0000, 0x0000, 0x0000, 0x01E0, 0x07F8, 0x0FF8, 0x0F3C, 0x1E1C, 0x1E00, 0x1C00, 0x1DFE, 0x1DFF, 0x1DFE, 0x1CFE, 0x1E1E, 0x1E1E, 0x0F3E, 0x0FFE, 0x07FE, 0x01FC, 0x0000, 0x0000}, /* G */
    {0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3FFE, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E}, /* H */
    {0x0FF8, 0x1FFC, 0x3FFE, 0x1FFC, 0x0FF8, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x0FF8, 0x1FFC, 0x3FFE, 0x1FFC}, /* I */
    {0x0FFC, 0x1FFE, 0x3FFE, 0x1FFE, 0x0FFC, 0x007C, 0x007C, 0x007C, 0x007C, 0x007C, 0x007C, 0x007C, 0x01FC, 0x07FC, 0x0E7C, 0x0C7C, 0x1C78, 0x0FF8, 0x0FF0, 0x07E0, 0x0180, 0x0000}, /* J */
    {0x3C04, 0x3C1E, 0x3C3F, 0x3C3E, 0x3C7E, 0x3CFC, 0x3DF8, 0x3FF0, 0x3FE0, 0x3FC0, 0x3F80, 0x3F80, 0x3FC0, 0x3FE0, 0x3FF0, 0x3DF0, 0x3CF8, 0x3CFC, 0x3C7E, 0x3C3F, 0x3C1F, 0x3C0F}, /* K */
    {0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3FFE, 0x3FFF, 0x3FFF, 0x3FFF}, /* L */
    {0x3006, 0x780F, 0x780F, 0x780F, 0x7C1F, 0x7C1F, 0x7E3F, 0x7F7F, 0x7F7F, 0x7FFF, 0x7FFF, 0x7FFF, 0x7BEF, 0x7BEF, 0x79CF, 0x780F, 0x780F, 0x780F, 0x780F, 0x780F, 0x780F, 0x780F}, /* M */
    {0x380E, 0x3C1E, 0x7C1F, 0x7E1F, 0x7E1F, 0x7F1F, 0x7F1F, 0x7F9F, 0x7F9F, 0x7FDF, 0x7FDF, 0x7FFF, 0x7DFF, 0x7DFF, 0x7CFF, 0x7CFF, 0x7C7F, 0x7C7F, 0x7C3F, 0x7C3F, 0x7C1F, 0x3C1E}, /* N */
    {0x0000, 0x0000, 0x0000, 0x03E0, 0x07F0, 0x0FF8, 0x1F7C, 0x1E3C, 0x3C1E, 0x3C1E, 0x3C1E, 0x380E, 0x3C1E, 0x3C1E, 0x3C1E, 0x1E3C, 0x1F7C, 0x0FF8, 0x07F0, 0x03E0, 0x0000, 0x0000}, /* O */
    {0x3C00, 0x3C00, 0x3DC0, 0x3FF0, 0x3FF8, 0x3E78, 0x3C3C, 0x3C3C, 0x3E78, 0x3FF8, 0x3FF0, 0x3FC0, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00, 0x3C00}, /* P */
    {0x0000, 0x0000, 0x0000, 0x03E0, 0x07F0, 0x0FF8, 0x1F7C, 0x1E3C, 0x3C1E, 0x3C1E, 0x3C1E, 0x380E, 0x3C1E, 0x3C7E, 0x3CFE, 0x1EFC, 0x1F7C, 0x0FFC, 0x07FE, 0x03FF, 0x000F, 0x0007}, /* Q */
    {0x3C00, 0x3C00, 0x3DC0, 0x3FF0, 0x3FF8, 0x3E78, 0x3C3C, 0x3C3C, 0x3E78, 0x3FF8, 0x3FF0, 0x3FE0, 0x3FE0, 0x3DF0, 0x3DF0, 0x3CF8, 0x3CFC, 0x3C7C, 0x3C3E, 0x3C3F, 0x3C1F, 0x3C0F}, /* R */
    {0x0000, 0x0000, 0x01E0, 0x07F8, 0x0FFC, 0x0F1C, 0x0E0E, 0x0F1E, 0x0FFC, 0x0FF8, 0x0FF0, 0x03F8, 0x0FFC, 0x1FF8, 0x1E78, 0x3C38, 0x3838, 0x1C78, 0x1FF8, 0x0FF0, 0x0380, 0x0000}, /* S */
    {0x01C0, 0xFFFF, 0xFFFF, 0xFFFF, 0x7FFF, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0}, /* T */
    {0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3C1E, 0x3FFE, 0x3FFE, 0x3E3E, 0x3C1E, 0x3C1E, 0x3E3E, 0x1FFC, 0x0FF8, 0x07F0, 0x03E0, 0x0000, 0x0000}, /* U */
    {0x7007, 0x780F, 0x780F, 0x780F, 0x7C1F, 0x7C1F, 0x3C1E, 0x3E3E, 0x3E3E, 0x1E3C, 0x1F7C, 0x1F7C, 0x0F78, 0x0FF8, 0x0FF8, 0x07F0, 0x07F0, 0x07F0, 0x03E0, 0x03E0, 0x03E0, 0x03E0}, /* V */
    {0x6003, 0xF007, 0xF007, 0xF007, 0xF007, 0xF007, 0x71C7, 0x79CF, 0x7BEF, 0x7BEF, 0x7BEF, 0x7BEF, 0x3FFE, 0x3FFE, 0x3FFE, 0x3F7E, 0x3F7E, 0x1F7C, 0x1F7C, 0x1E3C, 0x1E3C, 0x1E3C}, /* W */
    {0x7007, 0x780F, 0x780F, 0x7C1F, 0x7E3F, 0x3E3E, 0x1F7C, 0x1FFC, 0x0FF8, 0x07F0, 0x07F0, 0x03E0, 0x07F0, 0x07F0, 0x0FF8, 0x1FFC, 0x1F7C, 0x3E3E, 0x7E3F, 0x7C1F, 0x780F, 0x780F}, /* X */
    {0x7007, 0x780F, 0x7C1F, 0x7C1F, 0x3E3E, 0x3E3E, 0x1F7C, 0x1FFC, 0x0FF8, 0x07F0, 0x07F0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0}, /* Y */
    {0x0080, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x003E, 0x007C, 0x00FC, 0x00F8, 0x01F0, 0x03F0, 0x03E0, 0x07C0, 0x07C0, 0x0F80, 0x1F80, 0x1F00, 0x3E00, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF}, /* Z */
    {0x0000, 0x0000, 0x0000, 0x03E0, 0x07F8, 0x0FF8, 0x1F7C, 0x1E7C, 0x3CFE, 0x3CFE, 0x3DFE, 0x39CE, 0x3FDE, 0x3F9E, 0x3F9E, 0x1F3C, 0x1F7C, 0x0FF8, 0x0FF0, 0x03E0, 0x0000, 0x0000}, /* 0 */
    {0x01E0, 0x03E0, 0x07E0, 0x0FE0, 0x1FE0, 0x1FE0, 0x1FE0, 0x1FE0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x03E0, 0x0FFC, 0x1FFE, 0x3FFE, 0x1FFE}, /* 1 */
    {0x0000, 0x0000, 0x01E0, 0x07F8, 0x0F3C, 0x0E1C, 0x1E1C, 0x0FFC, 0x0FFC, 0x07FC, 0x01FC, 0x01F8, 0x03F0, 0x03F0, 0x07E0, 0x0FC0, 0x1F80, 0x3F00, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF}, /* 2 */
    {0x0000, 0x0000, 0x01C0, 0x07F0, 0x0FF8, 0x0E3C, 0x1C3C, 0x1E3C, 0x0FFC, 0x07F8, 0x03E0, 0x01C0, 0x07F0, 0x0FF8, 0x1E7C, 0x1C3C, 0x1C3C, 0x1E7C, 0x0FF8, 0x07F0, 0x03E0, 0x0000}, /* 3 */
    {0x0078, 0x00F8, 0x01FC, 0x01FC, 0x03FC, 0x07FC, 0x07FC, 0x0FFC, 0x0FFC, 0x1F7C, 0x3F7C, 0x3E7C, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x007C, 0x007C, 0x007C, 0x007C, 0x007C, 0x0078}, /* 4 */
    {0x0000, 0x3FFE, 0x3FFE, 0x3FFE, 0x1FFE, 0x1E00, 0x1E00, 0x1E00, 0x1E00, 0x1E00, 0x1E00, 0x1FE0, 0x1FF8, 0x0FFC, 0x1E3C, 0x1C1C, 0x1E1C, 0x1F3C, 0x0FF8, 0x07F0, 0x01C0, 0x0000}, /* 5 */
    {0x0000, 0x03C0, 0x03C0, 0x03C0, 0x07C0, 0x0780, 0x0780, 0x0F80, 0x0FE0, 0x0FF0, 0x1FF8, 0x1FFC, 0x1E3C, 0x1E1C, 0x3C1E, 0x1C1C, 0x1E3C, 0x1FFC, 0x0FF8, 0x07F0, 0x03E0, 0x0000}, /* 6 */
    {0x0080, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x003E, 0x003C, 0x007C, 0x007C, 0x0078, 0x00F8, 0x00F8, 0x01F0, 0x01F0, 0x01E0, 0x03E0, 0x03E0, 0x03C0, 0x07C0, 0x07C0, 0x0780, 0x0780}, /* 7 */
    {0x0000, 0x0000, 0x01C0, 0x07F0, 0x0FF8, 0x1E3C, 0x1E3C, 0x1E3C, 0x0FF8, 0x0FF8, 0x03E0, 0x01C0, 0x07F0, 0x0FF8, 0x1E3C, 0x1C1C, 0x1C1C, 0x1E3C, 0x0FF8, 0x0FF8, 0x03E0, 0x0000}, /* 8 */
    {0x0000, 0x0000, 0x01C0, 0x07F0, 0x0FF8, 0x1FFC, 0x1E3C, 0x1C3C, 0x1C3E, 0x1E3C, 0x1FFC, 0x0FFC, 0x07F8, 0x00F8, 0x00F8, 0x00F8, 0x00F0, 0x01F0, 0x01F0, 0x01E0, 0x01E0, 0x01E0}, /* 9 */
};


static int bold_on(int glyph, int col, int row)
{
    if (glyph < 0 || glyph > 35 || col < 0 || col >= 16 || row < 0 || row >= 22) return 0;
    return (FONT_BOLD[glyph][row] >> (15 - col)) & 1;
}

int sand_bold_pixel(int glyph, int col, int row)
{
    if (glyph < 0 || glyph > 25) glyph = 0;
    return bold_on(glyph, col, row);
}


static void grain_spawn_rise(sand_timer_t *timer, sand_grain_t *grain, bool from_bottom)
{
    grain->size = 4 + (int)(rng_next(&timer->rng) % 3u);
    int max_x = SAND_SCREEN_W - grain->size;
    if (max_x < 0) max_x = 0;
    grain->x = rng_span(&timer->rng, 0, max_x);
    if (from_bottom) {
        grain->y = SAND_SCREEN_H + rng_span(&timer->rng, 0, 48);
    } else {
        int top = RISE_CLEAR_Y;
        int bottom = SAND_SCREEN_H - grain->size;
        if (bottom < top) bottom = top;
        grain->y = rng_span(&timer->rng, top, bottom);
    }
    grain->speed_px_per_s = 70 + rng_span(&timer->rng, 0, 90);
    grain->shade = (int)(rng_next(&timer->rng) % 3u);
    grain->alive = true;
    grain->start_x = grain->x;
    grain->start_y = grain->y;
}

static void grain_spawn_fall(sand_timer_t *timer, sand_grain_t *grain, bool from_top)
{
    grain->size = 4 + (int)(rng_next(&timer->rng) % 3u);
    int max_x = SAND_SCREEN_W - grain->size - 4;
    if (max_x < 4) max_x = 4;
    grain->x = rng_span(&timer->rng, 4, max_x);
    if (from_top) {
        grain->y = -rng_span(&timer->rng, 6, 28);
    } else {
        grain->y = rng_span(&timer->rng, 48, 180);
    }
    grain->speed_px_per_s = 80 + rng_span(&timer->rng, 0, 110);
    grain->shade = (int)(rng_next(&timer->rng) % 3u);
    grain->alive = true;
    grain->start_x = grain->x;
    grain->start_y = grain->y;
}


static void grains_reset_fall(sand_timer_t *timer)
{
    timer->rng = 0xC3A5u ^ (uint32_t)(timer->preset + 1) * 0x9E3779B9u;
    if (timer->rng == 0) timer->rng = 0x6D2B79F5u;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        grain_spawn_fall(timer, &timer->grains[i], false);
    }
}

void sand_sprite_size(int kind, int shape, int *w, int *h)
{
    int width = 10;
    int height = 10;
    if (kind == SAND_KIND_LETTER) {
        width = 8;
        height = 11;
    } else if (shape == SAND_CHOICE_TRIANGLE || shape == SAND_CHOICE_HEXAGON) {
        width = 12;
        height = 11;
    } else if (shape == SAND_CHOICE_DIAMOND) {
        width = 11;
        height = 11;
    }     else if (shape == SAND_CHOICE_RECT) {
        width = 12;
        height = 7;
    }
    if (w) *w = width;
    if (h) *h = height;
}

static uint32_t pile_release_window(uint32_t duration_ms)
{
    /* Last pieces leave early enough to land as the countdown reaches zero. */
    if (duration_ms > 1800u) return duration_ms - 1400u;
    return duration_ms / 2u;
}

static void release_piece(sand_timer_t *timer, sand_grain_t *grain)
{
    int shape = timer->kind == SAND_KIND_LETTER ? timer->letter : timer->form_choice;
    int turn = rng_span(&timer->rng, 0, 7);
    if (shape < 0) shape = 0;
    /* Same small cell for every shape, so the pile reaches the top together.
     * The drawn sprite still uses the real shape and a random angle. */
    grain->size = 10;
    grain->ly = 10;
    grain->tone = (int8_t)shape;
    grain->spin = (int8_t)turn;
    grain->lz = (int16_t)(timer->kind == SAND_KIND_LETTER ? timer->color : timer->material);
    if (grain->x > SAND_SCREEN_W - grain->size - 1) grain->x = SAND_SCREEN_W - grain->size - 1;
    if (grain->x < 1) grain->x = 1;
    grain->y = -grain->ly - rng_span(&timer->rng, 4, 36);
    grain->start_y = 0;
    grain->fx = 0;
    grain->alive = true;
}

static void grains_reset_pile(sand_timer_t *timer)
{
    int base_w = 18;
    int base_h = 18;
    uint32_t span = timer->duration_ms;
    uint32_t window;
    int drift;
    timer->rng = 0x51A7u ^ (uint32_t)(timer->form_choice + 1) * 0x9E3779B9u
        ^ (uint32_t)(timer->form_time + 3) * 0x85EBCA6Bu ^ timer->entropy
        ^ (uint32_t)(timer->letter + 1) * 0x27BB2EE6u;
    if (timer->rng == 0) timer->rng = 0x6D2B79F5u;
    if (span < 1000u) span = 1000u;
    window = pile_release_window(span);
    sky_reset();
    drift = 56;
    sand_sprite_size(timer->kind, timer->kind == SAND_KIND_LETTER ? timer->letter : timer->form_choice,
                     &base_w, &base_h);
    for (int i = 0; i < SAND_FORM_DOTS; i++) timer->grains[i].alive = false;
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        sand_grain_t *grain = &timer->grains[i];
        int slot = (int)(((uint32_t)i * window) / SAND_PILE_COUNT);
        int jitter = rng_span(&timer->rng, 0, (int)(window / SAND_PILE_COUNT));
        grain->size = base_w;
        grain->ly = (int16_t)base_h;
        grain->x = rng_span(&timer->rng, 2, SAND_SCREEN_W - grain->size - 2);
        grain->y = -grain->ly - rng_span(&timer->rng, 4, 40);
        grain->lx = (int16_t)rng_span(&timer->rng, -drift, drift);
        grain->speed_px_per_s = rng_span(&timer->rng, 36, 96);
        grain->shade = rng_span(&timer->rng, 0, 2);
        grain->mark = 0;
        grain->alive = false;
        grain->start_x = slot + jitter / 2;
        grain->start_y = 0;
        grain->fx = 0;
        grain->tone = -1;
        grain->spin = 0;
        grain->lz = 0;
        if (grain->start_x < 0) grain->start_x = 0;
    }
}

static void grains_reset_rise(sand_timer_t *timer)
{
    timer->rng = 0xA71Du ^ 0x9E3779B9u;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        grain_spawn_rise(timer, &timer->grains[i], false);
    }
}

static void grains_stop(sand_timer_t *timer)
{
    for (int i = 0; i < SAND_FORM_DOTS; i++) {
        timer->grains[i].alive = false;
    }
}

static void cycle_color(sand_timer_t *timer, int direction)
{
    int count = SAND_COLOR_COUNT;
    timer->color = (timer->color + direction + count * 4) % count;
}

static void cycle_material(sand_timer_t *timer, int direction)
{
    int count = SAND_MATERIAL_COUNT;
    timer->material = (timer->material + direction + count * 4) % count;
}

static void cycle_shape(sand_timer_t *timer, int direction)
{
    int count = SAND_CHOICE_COUNT;
    timer->form_choice = (timer->form_choice + direction + count * 4) % count;
    timer->form = (sand_form_t)timer->form_choice;
}

static void cycle_letter(sand_timer_t *timer, int direction)
{
    timer->letter = (timer->letter + direction + 26 * 4) % 26;
}

static void play_adjust(sand_timer_t *timer, sand_input_t input)
{
    if (timer->kind == SAND_KIND_FORM) {
        if (input == SAND_INPUT_UP) cycle_shape(timer, +1);
        else if (input == SAND_INPUT_DOWN) cycle_material(timer, +1);
        return;
    }
    if (timer->kind == SAND_KIND_LETTER) {
        if (input == SAND_INPUT_UP) cycle_letter(timer, +1);
        else if (input == SAND_INPUT_DOWN) cycle_color(timer, +1);
        return;
    }
    if (input == SAND_INPUT_UP) cycle_color(timer, +1);
    else if (input == SAND_INPUT_DOWN) cycle_color(timer, -1);
}

static void remember_duration(sand_timer_t *timer)
{
    timer->duration_ms = sand_preset_duration_ms(timer->preset);
}

static void begin_run(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_POMODORO;
    remember_duration(timer);
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->phase = SAND_PHASE_RUNNING;
    grains_reset_fall(timer);
}

static void begin_form(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_FORM;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = sand_form_time_ms(timer->form_time);
    timer->phase = SAND_PHASE_RUNNING;
    timer->form = (sand_form_t)timer->form_choice;
    grains_reset_pile(timer);
}

static bool gathers(const sand_timer_t *timer)
{
    return timer->kind == SAND_KIND_FORM || timer->kind == SAND_KIND_LETTER;
}

static void open_setup(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_FORM;
    timer->phase = SAND_PHASE_SETUP;
    timer->setup_row = SAND_ROW_START;
    timer->typing = false;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = sand_form_time_ms(timer->form_time);
    grains_stop(timer);
}

static void open_letter(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_LETTER;
    timer->phase = SAND_PHASE_SETUP;
    timer->setup_row = SAND_LROW_GLYPH;
    timer->typing = false;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = sand_form_time_ms(timer->form_time);
    grains_stop(timer);
}

static void begin_letter(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_LETTER;
    timer->typing = false;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = sand_form_time_ms(timer->form_time);
    timer->phase = SAND_PHASE_RUNNING;
    timer->form = SAND_FORM_LETTER;
    grains_reset_pile(timer);
}


static void back_to_pick(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_POMODORO;
    timer->phase = SAND_PHASE_PICK;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    remember_duration(timer);
    grains_stop(timer);
}

static void back_to_menu(sand_timer_t *timer)
{
    timer->phase = SAND_PHASE_MENU;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = 0;
    grains_stop(timer);
}

static void begin_stopwatch(sand_timer_t *timer)
{
    timer->kind = SAND_KIND_STOPWATCH;
    timer->elapsed_ms = 0;
    timer->overtime_ms = 0;
    timer->duration_ms = 0;
    timer->phase = SAND_PHASE_RUNNING;
    grains_reset_rise(timer);
}

uint32_t sand_preset_duration_ms(int preset)
{
    if (preset < 0 || preset >= SAND_PRESET_COUNT) return 0;
    return PRESET_MS[preset];
}

int sand_form_time_min(int index)
{
    if (index < 0 || index >= SAND_FORM_TIME_COUNT) return 0;
    return FORM_MIN[index];
}

uint32_t sand_form_time_ms(int index)
{
    int minutes = sand_form_time_min(index);
    return (uint32_t)minutes * 60u * 1000u;
}

int sand_fill_px(uint32_t elapsed_ms, uint32_t duration_ms, int screen_h)
{
    if (screen_h <= 0) return 0;
    if (duration_ms == 0 || elapsed_ms >= duration_ms) return screen_h;
    return (int)(((uint64_t)elapsed_ms * (uint64_t)screen_h) / duration_ms);
}

uint32_t sand_remaining_ms(const sand_timer_t *timer)
{
    if (timer->elapsed_ms >= timer->duration_ms) return 0;
    return timer->duration_ms - timer->elapsed_ms;
}

static void format_mmss(uint32_t ms, char *buf, size_t len)
{
    uint32_t total_s = ms / 1000u;
    unsigned mm = (unsigned)(total_s / 60u);
    unsigned ss = (unsigned)(total_s % 60u);
    if (mm > 99u) mm = 99u;
    snprintf(buf, len, "%02u:%02u", mm, ss);
}

void sand_format_remaining(const sand_timer_t *timer, char *buf, size_t len)
{
    format_mmss(sand_remaining_ms(timer), buf, len);
}

void sand_format_clock(const sand_timer_t *timer, char *buf, size_t len)
{
    if (timer->kind == SAND_KIND_STOPWATCH) format_mmss(timer->elapsed_ms, buf, len);
    else if (timer->phase == SAND_PHASE_DONE) format_mmss(timer->overtime_ms, buf, len);
    else sand_format_remaining(timer, buf, len);
}

void sand_format_soc(int soc, char *buf, size_t len)
{
    if (!buf || len == 0) return;
    if (soc < 0) {
        buf[0] = '\0';
        return;
    }
    if (soc > 100) soc = 100;
    snprintf(buf, len, "%d%%", soc);
}

uint32_t sand_color_rgb(int color, int shade)
{
    if (color < 0 || color >= SAND_COLOR_COUNT) color = 0;
    if (shade < 0) shade = 0;
    if (shade > 2) shade = 2;
    return COLOR_RGB[color][shade];
}

void sand_timer_set_entropy(sand_timer_t *timer, uint32_t entropy)
{
    timer->entropy = entropy;
}

void sand_form_name(const sand_timer_t *timer, char *buf, size_t len)
{
    if (!buf || len == 0) return;
    switch (timer->form) {
    case SAND_FORM_CIRCLE: snprintf(buf, len, "CIRCLE"); break;
    case SAND_FORM_SQUARE: snprintf(buf, len, "SQUARE"); break;
    case SAND_FORM_TRIANGLE: snprintf(buf, len, "TRIANGLE"); break;
    case SAND_FORM_DIAMOND: snprintf(buf, len, "DIAMOND"); break;
    case SAND_FORM_HEXAGON: snprintf(buf, len, "HEXAGON"); break;
    case SAND_FORM_RECT: snprintf(buf, len, "RECTANGLE"); break;
    case SAND_FORM_LETTER:
        if (timer->custom_len > 0) snprintf(buf, len, "%s", timer->custom);
        else {
            int letter = timer->letter;
            if (letter < 0 || letter > 25) letter = 0;
            snprintf(buf, len, "LETTER %c", 'A' + letter);
        }
        break;
    default:
        snprintf(buf, len, "FORM");
        break;
    }
}

void sand_choice_name(int choice, char *buf, size_t len)
{
    const char *name = SAND_STR_FORM;
    if (!buf || len == 0) return;
    switch (choice) {
    case SAND_CHOICE_CIRCLE: name = SAND_STR_CIRCLE; break;
    case SAND_CHOICE_SQUARE: name = SAND_STR_SQUARE; break;
    case SAND_CHOICE_TRIANGLE: name = SAND_STR_TRIANGLE; break;
    case SAND_CHOICE_DIAMOND: name = SAND_STR_DIAMOND; break;
    case SAND_CHOICE_HEXAGON: name = SAND_STR_HEXAGON; break;
    case SAND_CHOICE_RECT: name = SAND_STR_RECT; break;
    default: break;
    }
    snprintf(buf, len, "%s", name);
}

char sand_type_char(int index)
{
    static const char reel[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789<";
    int count = (int)(sizeof(reel) - 1u);
    if (count <= 0) return 'A';
    if (index < 0) index = 0;
    return reel[index % count];
}

void sand_material_name(int material, char *buf, size_t len)
{
    const char *name = SAND_STR_FIRE;
    if (!buf || len == 0) return;
    switch (material) {
    case SAND_MAT_WATER: name = SAND_STR_WATER; break;
    case SAND_MAT_STONE: name = SAND_STR_STONE; break;
    case SAND_MAT_YUAN: name = SAND_STR_YUAN; break;
    case SAND_MAT_WOOD: name = SAND_STR_WOOD; break;
    case SAND_MAT_FIRE:
    default: name = SAND_STR_FIRE; break;
    }
    snprintf(buf, len, "%s", name);
}

uint32_t sand_material_rgb(int material, int shade)
{
    if (material < 0 || material >= SAND_MATERIAL_COUNT) material = 0;
    if (shade < 0) shade = 0;
    if (shade > 2) shade = 2;
    return MATERIAL_RGB[material][shade];
}

int sand_light_percent(int level)
{
    static const int percent[5] = {20, 40, 60, 80, 100};
    if (level < 1) level = 1;
    if (level > 5) level = 5;
    return percent[level - 1];
}

int sand_sound_percent(int level)
{
    static const int percent[3] = {30, 55, 85};
    if (level < 1) level = 1;
    if (level > 3) level = 3;
    return percent[level - 1];
}

void sand_form_target(const sand_timer_t *timer, int index, uint32_t spin_ms,
                      int *x, int *y, int *size, int *shade)
{
    const sand_grain_t *grain;
    (void)spin_ms;
    if (index < 0) index = 0;
    if (index >= SAND_FORM_DOTS) index = SAND_FORM_DOTS - 1;
    grain = &timer->grains[index];
    if (x) *x = grain->x;
    if (y) *y = grain->y;
    if (size) *size = grain->size;
    if (shade) *shade = grain->shade;
}

void sand_timer_init(sand_timer_t *timer)
{
    memset(timer, 0, sizeof(*timer));
    timer->phase = SAND_PHASE_MENU;
    timer->menu = SAND_KIND_FORM;
    timer->kind = SAND_KIND_FORM;
    timer->preset = 0;
    timer->color = 0;
    timer->material = SAND_MAT_YUAN;
    timer->form_time = 3;
    timer->form_choice = SAND_CHOICE_DIAMOND;
    timer->setup_row = SAND_ROW_START;
    timer->light_level = 3;
    timer->sound_level = 2;
    timer->rng = 1;
    remember_duration(timer);
}

void sand_timer_on_input(sand_timer_t *timer, sand_input_t input)
{
    if (timer->phase == SAND_PHASE_MENU && input == SAND_INPUT_UP_LONG) {
        timer->light_level = timer->light_level >= 5 ? 1 : timer->light_level + 1;
        timer->dial = timer->light_level;
        timer->dial_ms = SAND_DIAL_MS;
        return;
    }
    if (timer->phase == SAND_PHASE_MENU && input == SAND_INPUT_DOWN_LONG) {
        timer->sound_level = timer->sound_level >= 3 ? 1 : timer->sound_level + 1;
        timer->dial = timer->sound_level;
        timer->dial_ms = SAND_DIAL_MS;
        return;
    }
    if (input == SAND_INPUT_UP_LONG) {
        if (timer->kind == SAND_KIND_FORM && timer->phase != SAND_PHASE_MENU
            && timer->phase != SAND_PHASE_PICK) {
            cycle_material(timer, +1);
        } else {
            cycle_color(timer, +1);
        }
        return;
    }

    switch (timer->phase) {
    case SAND_PHASE_MENU:
        if (input == SAND_INPUT_UP) {
            timer->menu = (timer->menu + SAND_MENU_COUNT - 1) % SAND_MENU_COUNT;
        } else if (input == SAND_INPUT_DOWN) {
            timer->menu = (timer->menu + 1) % SAND_MENU_COUNT;
        } else if (input == SAND_INPUT_OK) {
            if (timer->menu == SAND_KIND_STOPWATCH) begin_stopwatch(timer);
            else if (timer->menu == SAND_KIND_FORM) open_setup(timer);
            else if (timer->menu == SAND_KIND_LETTER) open_letter(timer);
            else back_to_pick(timer);
        }
        break;
    case SAND_PHASE_PICK:
        if (input == SAND_INPUT_UP) {
            timer->preset = (timer->preset + SAND_PRESET_COUNT - 1) % SAND_PRESET_COUNT;
            remember_duration(timer);
        } else if (input == SAND_INPUT_DOWN) {
            timer->preset = (timer->preset + 1) % SAND_PRESET_COUNT;
            remember_duration(timer);
        } else if (input == SAND_INPUT_OK) {
            begin_run(timer);
        } else if (input == SAND_INPUT_OK_LONG) {
            back_to_menu(timer);
        }
        break;
    case SAND_PHASE_SETUP:
        if (timer->kind == SAND_KIND_LETTER) {
            if (input == SAND_INPUT_UP) {
                timer->setup_row = (timer->setup_row + SAND_LROW_COUNT - 1) % SAND_LROW_COUNT;
            } else if (input == SAND_INPUT_DOWN) {
                timer->setup_row = (timer->setup_row + 1) % SAND_LROW_COUNT;
            } else if (input == SAND_INPUT_OK) {
                if (timer->setup_row == SAND_LROW_GLYPH) {
                    cycle_letter(timer, +1);
                } else if (timer->setup_row == SAND_LROW_TIME) {
                    timer->form_time = (timer->form_time + 1) % SAND_FORM_TIME_COUNT;
                    timer->duration_ms = sand_form_time_ms(timer->form_time);
                } else if (timer->setup_row == SAND_LROW_COLOR) {
                    cycle_color(timer, +1);
                } else {
                    begin_letter(timer);
                }
            } else if (input == SAND_INPUT_OK_LONG) {
                back_to_menu(timer);
            }
            break;
        }
        if (input == SAND_INPUT_UP) {
            timer->setup_row = (timer->setup_row + SAND_ROW_COUNT - 1) % SAND_ROW_COUNT;
        } else if (input == SAND_INPUT_DOWN) {
            timer->setup_row = (timer->setup_row + 1) % SAND_ROW_COUNT;
        } else if (input == SAND_INPUT_OK) {
            if (timer->setup_row == SAND_ROW_TIME) {
                timer->form_time = (timer->form_time + 1) % SAND_FORM_TIME_COUNT;
                timer->duration_ms = sand_form_time_ms(timer->form_time);
            } else if (timer->setup_row == SAND_ROW_SHAPE) {
                cycle_shape(timer, +1);
            } else if (timer->setup_row == SAND_ROW_COLOR) {
                cycle_material(timer, +1);
            } else {
                begin_form(timer);
            }
        } else if (input == SAND_INPUT_OK_LONG) {
            back_to_menu(timer);
        }
        break;
    case SAND_PHASE_RUNNING:
        if (input == SAND_INPUT_OK) timer->phase = SAND_PHASE_PAUSED;
        else if (input == SAND_INPUT_OK_LONG) {
            if (timer->kind == SAND_KIND_STOPWATCH) back_to_menu(timer);
            else if (timer->kind == SAND_KIND_LETTER) open_letter(timer);
            else if (timer->kind == SAND_KIND_FORM) open_setup(timer);
            else back_to_pick(timer);
        } else {
            play_adjust(timer, input);
        }
        break;
    case SAND_PHASE_PAUSED:
        if (input == SAND_INPUT_OK) timer->phase = SAND_PHASE_RUNNING;
        else if (input == SAND_INPUT_OK_LONG) {
            if (timer->kind == SAND_KIND_STOPWATCH) back_to_menu(timer);
            else if (timer->kind == SAND_KIND_LETTER) open_letter(timer);
            else if (timer->kind == SAND_KIND_FORM) open_setup(timer);
            else back_to_pick(timer);
        } else {
            play_adjust(timer, input);
        }
        break;
    case SAND_PHASE_DONE:
        if (input == SAND_INPUT_OK || input == SAND_INPUT_OK_LONG) {
            if (timer->kind == SAND_KIND_LETTER) open_letter(timer);
            else if (timer->kind == SAND_KIND_FORM) open_setup(timer);
            else back_to_pick(timer);
        } else {
            play_adjust(timer, input);
        }
        break;
    }
}

#define PILE_FLOOR (SAND_SCREEN_H - 8)

static void pile_clamp_x(sand_grain_t *grain)
{
    if (grain->x < 1) {
        grain->x = 1;
        grain->lx = (int16_t)(-grain->lx / 2);
    }
    if (grain->x + grain->size > SAND_SCREEN_W - 1) {
        grain->x = SAND_SCREEN_W - 1 - grain->size;
        grain->lx = (int16_t)(-grain->lx / 2);
    }
}

static int16_t s_sky[SAND_SCREEN_W];

static void sky_reset(void)
{
    for (int x = 0; x < SAND_SCREEN_W; x++) s_sky[x] = PILE_FLOOR;
}

static void sky_stamp(const sand_grain_t *grain)
{
    int x0 = grain->x;
    int x1 = grain->x + grain->size;
    if (x0 < 0) x0 = 0;
    if (x1 > SAND_SCREEN_W) x1 = SAND_SCREEN_W;
    for (int x = x0; x < x1; x++) {
        if (grain->y < s_sky[x]) s_sky[x] = (int16_t)grain->y;
    }
}

static int sky_top(const sand_grain_t *grain)
{
    int x0 = grain->x;
    int x1 = grain->x + grain->size;
    int surface = PILE_FLOOR;
    if (x0 < 0) x0 = 0;
    if (x1 > SAND_SCREEN_W) x1 = SAND_SCREEN_W;
    for (int x = x0; x < x1; x++) {
        if (s_sky[x] < surface) surface = s_sky[x];
    }
    return surface;
}

static void pile_separate(sand_timer_t *timer, int index)
{
    sand_grain_t *grain = &timer->grains[index];
    int top;
    if (!grain->alive || grain->mark) return;
    pile_clamp_x(grain);
    if (grain->y + grain->ly < sky_top(grain)) return;
    for (int slide = 0; slide < 6; slide++) {
        int current = sky_top(grain);
        int best_x = grain->x;
        int best = current;
        int step = grain->size > 6 ? grain->size / 2 : 4;
        int saved = grain->x;
        for (int dir = -1; dir <= 1; dir += 2) {
            int side;
            grain->x = saved + dir * step;
            pile_clamp_x(grain);
            if (grain->x == saved) continue;
            side = sky_top(grain);
            if (side > best + 3) {
                best = side;
                best_x = grain->x;
            }
        }
        grain->x = best_x;
        pile_clamp_x(grain);
        if (grain->x == saved) break;
    }
    top = sky_top(grain);
    if (grain->y + grain->ly < top) return;
    if (grain->y > top - grain->ly) {
        int gap = top < 180 ? 1 : 0;
        grain->y = top - grain->ly - gap;
    }
    grain->mark = 1;
    grain->speed_px_per_s = 0;
    grain->lx = 0;
    sky_stamp(grain);
}

static void tick_pile(sand_timer_t *timer, uint32_t dt_ms)
{
    uint32_t step = dt_ms > 80u ? 80u : dt_ms;
    int active[96];
    int count = 0;
    int overflow = 0;
    if (step < 1u) step = 1u;
    for (int i = 0; i < SAND_PILE_COUNT; i++) {
        sand_grain_t *grain = &timer->grains[i];
        int dy;
        int dx;
        if (!grain->alive) {
            if ((uint32_t)grain->start_x <= timer->elapsed_ms) release_piece(timer, grain);
            else continue;
        }
        if (grain->mark) continue;
        grain->speed_px_per_s += (int)((640u * step) / 1000u);
        if (grain->speed_px_per_s > 280) grain->speed_px_per_s = 280;
        dy = grain->speed_px_per_s * (int)step + grain->start_y;
        grain->y += dy / 1000;
        grain->start_y = dy % 1000;
        dx = (int)grain->lx * (int)step + grain->fx;
        grain->x += dx / 1000;
        grain->fx = (int16_t)(dx % 1000);
        if (count < 96) active[count++] = i;
        else overflow = 1;
    }
    for (int i = 1; i < count; i++) {
        int key = active[i];
        int j = i;
        while (j > 0 && timer->grains[active[j - 1]].y < timer->grains[key].y) {
            active[j] = active[j - 1];
            j--;
        }
        active[j] = key;
    }
    for (int pass = 0; pass < 2; pass++) {
        for (int n = 0; n < count; n++) pile_separate(timer, active[n]);
    }
    if (overflow) {
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (timer->grains[i].alive && !timer->grains[i].mark) pile_separate(timer, i);
        }
    }
}

static void tick_fall(sand_timer_t *timer, uint32_t dt_ms)
{
    int fill = sand_fill_px(timer->elapsed_ms, timer->duration_ms, SAND_SCREEN_H);
    int surface = SAND_SCREEN_H - fill;
    uint32_t visual_ms = dt_ms > 80u ? 80u : dt_ms;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        sand_grain_t *grain = &timer->grains[i];
        if (!grain->alive) continue;
        grain->y += (int)(((int64_t)grain->speed_px_per_s * (int64_t)visual_ms) / 1000);
        if (grain->y + grain->size >= surface) grain_spawn_fall(timer, grain, true);
    }
}

static void tick_rise(sand_timer_t *timer, uint32_t dt_ms)
{
    uint32_t room = UINT32_MAX - timer->elapsed_ms;
    timer->elapsed_ms += dt_ms > room ? room : dt_ms;
    uint32_t visual_ms = dt_ms > 80u ? 80u : dt_ms;
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) {
        sand_grain_t *grain = &timer->grains[i];
        if (!grain->alive) continue;
        grain->y -= (int)(((int64_t)grain->speed_px_per_s * (int64_t)visual_ms) / 1000);
        if (grain->y < RISE_CLEAR_Y) grain_spawn_rise(timer, grain, true);
    }
}

void sand_timer_tick(sand_timer_t *timer, uint32_t dt_ms)
{
    if (timer->dial != 0) {
        if (dt_ms >= timer->dial_ms) {
            timer->dial = 0;
            timer->dial_ms = 0;
        } else {
            timer->dial_ms -= dt_ms;
        }
    }
    if (timer->phase == SAND_PHASE_DONE) {
        uint32_t room = UINT32_MAX - timer->overtime_ms;
        timer->overtime_ms += dt_ms > room ? room : dt_ms;
        return;
    }
    if (timer->phase != SAND_PHASE_RUNNING) return;
    if (timer->kind == SAND_KIND_STOPWATCH) {
        tick_rise(timer, dt_ms);
        return;
    }
    if (timer->duration_ms == 0) return;

    uint32_t remain = timer->duration_ms - timer->elapsed_ms;
    if (dt_ms >= remain) {
        timer->elapsed_ms = timer->duration_ms;
        if (gathers(timer)) tick_pile(timer, remain > 80u ? 80u : remain);
        else grains_stop(timer);
        timer->phase = SAND_PHASE_DONE;
        return;
    }

    timer->elapsed_ms += dt_ms;
    if (gathers(timer)) tick_pile(timer, dt_ms);
    else tick_fall(timer, dt_ms);
}

#include <assert.h>
#include <string.h>

#include "sand_timer.h"

static void test_presets_and_fill(void)
{
    assert(sand_preset_duration_ms(0) == 10u * 60u * 1000u);
    assert(sand_preset_duration_ms(1) == 20u * 60u * 1000u);
    assert(sand_preset_duration_ms(2) == 30u * 60u * 1000u);
    assert(sand_preset_duration_ms(-1) == 0);
    assert(sand_preset_duration_ms(3) == 0);

    assert(sand_fill_px(0, 600000, 320) == 0);
    assert(sand_fill_px(300000, 600000, 320) == 160);
    assert(sand_fill_px(599999, 600000, 320) == 319);
    assert(sand_fill_px(600000, 600000, 320) == 320);

    assert(sand_fill_px(600000, 1200000, 320) == 160);
    assert(sand_fill_px(1200000, 1200000, 320) == 320);
    assert(sand_fill_px(1200000, 1800000, 320) == 213);
    assert(sand_fill_px(1799999, 1800000, 320) == 319);
    assert(sand_fill_px(1800000, 1800000, 320) == 320);
    assert(sand_fill_px(1, 600000, 0) == 0);
    assert(sand_fill_px(0, 0, 320) == 320);
}

static void enter_pomodoro(sand_timer_t *timer)
{
    assert(timer->phase == SAND_PHASE_MENU);
    while (timer->menu != SAND_KIND_POMODORO) {
        sand_timer_on_input(timer, SAND_INPUT_DOWN);
    }
    sand_timer_on_input(timer, SAND_INPUT_OK);
    assert(timer->phase == SAND_PHASE_PICK);
    assert(timer->kind == SAND_KIND_POMODORO);
}

static void start_pomodoro(sand_timer_t *timer)
{
    enter_pomodoro(timer);
    sand_timer_on_input(timer, SAND_INPUT_OK);
    assert(timer->phase == SAND_PHASE_RUNNING);
    assert(timer->kind == SAND_KIND_POMODORO);
}

static void enter_form(sand_timer_t *timer)
{
    assert(timer->phase == SAND_PHASE_MENU);
    while (timer->menu != SAND_KIND_FORM) {
        sand_timer_on_input(timer, SAND_INPUT_DOWN);
    }
    sand_timer_on_input(timer, SAND_INPUT_OK);
    assert(timer->phase == SAND_PHASE_SETUP);
    assert(timer->kind == SAND_KIND_FORM);
}

static void start_form_choice(sand_timer_t *timer, int choice, uint32_t entropy)
{
    sand_timer_init(timer);
    sand_timer_set_entropy(timer, entropy);
    enter_form(timer);
    timer->form_choice = choice;
    timer->form_time = 0;
    timer->setup_row = SAND_ROW_START;
    sand_timer_on_input(timer, SAND_INPUT_OK);
    assert(timer->phase == SAND_PHASE_RUNNING);
    assert(timer->kind == SAND_KIND_FORM);
    assert(timer->form_choice == choice);
}

static void test_pick_and_clock(void)
{
    sand_timer_t timer;
    char clock[8];

    sand_timer_init(&timer);
    assert(timer.phase == SAND_PHASE_MENU);
    assert(timer.menu == SAND_KIND_FORM);
    assert(timer.preset == 0);
    sand_format_remaining(&timer, clock, sizeof(clock));
    assert(strcmp(clock, "10:00") == 0);
    enter_pomodoro(&timer);

    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    sand_format_remaining(&timer, clock, sizeof(clock));
    assert(timer.preset == 1);
    assert(strcmp(clock, "20:00") == 0);

    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    sand_format_remaining(&timer, clock, sizeof(clock));
    assert(strcmp(clock, "30:00") == 0);

    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.preset == 0);
    sand_timer_on_input(&timer, SAND_INPUT_UP);
    assert(timer.preset == 2);

    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    assert(timer.color == 1);
    assert(timer.preset == 2);
    assert(timer.phase == SAND_PHASE_PICK);
    for (int i = 0; i < SAND_COLOR_COUNT - 1; i++) {
        sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    }
    assert(timer.color == 0);
    sand_timer_on_input(&timer, SAND_INPUT_OK_LONG);
    assert(timer.phase == SAND_PHASE_MENU);
    assert(timer.preset == 2);
    assert(timer.color == 0);
}

static void test_run_pause_color_and_finish(void)
{
    sand_timer_t a;
    sand_timer_t b;
    char clock[8];

    sand_timer_init(&a);
    sand_timer_init(&b);
    start_pomodoro(&a);
    start_pomodoro(&b);
    assert(a.phase == SAND_PHASE_RUNNING);
    assert(a.grains[0].alive);
    assert(a.grains[0].x == b.grains[0].x);
    assert(a.grains[0].y == b.grains[0].y);
    assert(a.grains[3].speed_px_per_s == b.grains[3].speed_px_per_s);

    int y0 = a.grains[0].y;
    assert(a.grains[0].y > 0);
    assert(a.grains[0].y < SAND_SCREEN_H);
    sand_timer_tick(&a, 1000);
    assert(a.grains[0].y > y0);
    assert(a.grains[0].alive);

    sand_timer_on_input(&a, SAND_INPUT_OK);
    assert(a.phase == SAND_PHASE_PAUSED);
    uint32_t elapsed = a.elapsed_ms;
    int y = a.grains[0].y;
    sand_timer_tick(&a, 5000);
    assert(a.elapsed_ms == elapsed);
    assert(a.grains[0].y == y);

    sand_timer_on_input(&a, SAND_INPUT_OK);
    sand_timer_tick(&a, 50);
    assert(a.phase == SAND_PHASE_RUNNING);
    assert(a.elapsed_ms == elapsed + 50);

    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    assert(a.color == SAND_COLOR_COUNT - 1);
    assert(a.preset == 0);
    sand_timer_on_input(&a, SAND_INPUT_UP);
    assert(a.color == 0);

    sand_timer_init(&a);
    start_pomodoro(&a);
    sand_timer_tick(&a, 90000);
    sand_format_remaining(&a, clock, sizeof(clock));
    assert(strcmp(clock, "08:30") == 0);
    sand_format_soc(100, clock, sizeof(clock));
    assert(strcmp(clock, "100%") == 0);
    sand_format_soc(0, clock, sizeof(clock));
    assert(strcmp(clock, "0%") == 0);
    sand_format_soc(150, clock, sizeof(clock));
    assert(strcmp(clock, "100%") == 0);
    sand_format_soc(-1, clock, sizeof(clock));
    assert(clock[0] == '\0');

    sand_timer_tick(&a, 50);
    assert(a.grains[0].alive);
    assert(a.phase == SAND_PHASE_RUNNING);

    sand_timer_init(&a);
    enter_pomodoro(&a);
    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    sand_timer_on_input(&a, SAND_INPUT_OK);
    assert(a.duration_ms == 20u * 60u * 1000u);
    sand_timer_tick(&a, 10u * 60u * 1000u);
    assert(a.phase == SAND_PHASE_RUNNING);
    assert(sand_fill_px(a.elapsed_ms, a.duration_ms, SAND_SCREEN_H) == 160);

    sand_timer_tick(&a, 10u * 60u * 1000u);
    assert(a.phase == SAND_PHASE_DONE);
    assert(a.elapsed_ms == a.duration_ms);
    assert(sand_fill_px(a.elapsed_ms, a.duration_ms, SAND_SCREEN_H) == SAND_SCREEN_H);
    assert(sand_remaining_ms(&a) == 0);
    sand_format_remaining(&a, clock, sizeof(clock));
    assert(strcmp(clock, "00:00") == 0);
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) assert(!a.grains[i].alive);

    sand_timer_tick(&a, 1000);
    assert(a.phase == SAND_PHASE_DONE);
    assert(a.overtime_ms == 1000);
    sand_format_clock(&a, clock, sizeof(clock));
    assert(strcmp(clock, "00:01") == 0);
    sand_timer_tick(&a, 59000);
    sand_format_clock(&a, clock, sizeof(clock));
    assert(strcmp(clock, "01:00") == 0);
    sand_timer_on_input(&a, SAND_INPUT_UP);
    assert(a.color == 1);
    sand_timer_on_input(&a, SAND_INPUT_OK);
    assert(a.phase == SAND_PHASE_PICK);
    assert(a.elapsed_ms == 0);
    assert(a.overtime_ms == 0);
    assert(a.preset == 1);

    sand_timer_init(&a);
    start_pomodoro(&a);
    sand_timer_tick(&a, 5000);
    sand_timer_on_input(&a, SAND_INPUT_OK_LONG);
    assert(a.phase == SAND_PHASE_PICK);
    assert(a.elapsed_ms == 0);

    assert(sand_color_rgb(-1, 9) == sand_color_rgb(0, 2));
    assert(sand_color_rgb(0, 0) != sand_color_rgb(1, 1));
}

static void test_menu_and_stopwatch(void)
{
    sand_timer_t a;
    sand_timer_t b;
    char clock[8];

    sand_timer_init(&a);
    assert(a.menu == SAND_KIND_FORM);
    assert(a.phase == SAND_PHASE_MENU);
    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    assert(a.menu == SAND_KIND_LETTER);
    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    assert(a.menu == SAND_KIND_POMODORO);
    assert(a.preset == 0);
    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    assert(a.menu == SAND_KIND_STOPWATCH);
    sand_timer_on_input(&a, SAND_INPUT_DOWN);
    assert(a.menu == SAND_KIND_FORM);
    sand_timer_on_input(&a, SAND_INPUT_UP);
    assert(a.menu == SAND_KIND_STOPWATCH);

    sand_timer_init(&b);
    sand_timer_on_input(&b, SAND_INPUT_DOWN);
    sand_timer_on_input(&b, SAND_INPUT_DOWN);
    sand_timer_on_input(&b, SAND_INPUT_DOWN);
    sand_timer_on_input(&a, SAND_INPUT_OK);
    sand_timer_on_input(&b, SAND_INPUT_OK);
    assert(a.phase == SAND_PHASE_RUNNING);
    assert(a.kind == SAND_KIND_STOPWATCH);
    assert(a.elapsed_ms == 0);
    assert(a.grains[0].alive);
    assert(a.grains[0].y == b.grains[0].y);
    assert(a.grains[0].y >= 46);
    assert(a.grains[0].y < SAND_SCREEN_H);
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) assert(a.grains[i].y >= 46);

    int y0 = a.grains[0].y;
    int speed = a.grains[0].speed_px_per_s;
    sand_timer_tick(&a, 1000);
    assert(a.elapsed_ms == 1000);
    assert(a.phase == SAND_PHASE_RUNNING);
    assert(a.grains[0].y == y0 - (int)((int64_t)speed * 80 / 1000));
    sand_format_clock(&a, clock, sizeof(clock));
    assert(strcmp(clock, "00:01") == 0);
    assert(a.duration_ms == 0);

    sand_timer_on_input(&a, SAND_INPUT_OK);
    assert(a.phase == SAND_PHASE_PAUSED);
    y0 = a.grains[0].y;
    sand_timer_tick(&a, 5000);
    assert(a.elapsed_ms == 1000);
    assert(a.grains[0].y == y0);
    sand_timer_on_input(&a, SAND_INPUT_OK);
    assert(a.phase == SAND_PHASE_RUNNING);

    a.grains[0].y = -a.grains[0].size;
    sand_timer_tick(&a, 50);
    assert(a.grains[0].alive);
    assert(a.grains[0].y >= SAND_SCREEN_H);
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) assert(a.grains[i].alive);

    sand_timer_tick(&a, 120u * 60u * 1000u);
    assert(a.phase == SAND_PHASE_RUNNING);
    sand_format_clock(&a, clock, sizeof(clock));
    assert(strcmp(clock, "99:01") == 0);

    sand_timer_on_input(&a, SAND_INPUT_UP);
    assert(a.color == 1);
    sand_timer_on_input(&a, SAND_INPUT_OK_LONG);
    assert(a.phase == SAND_PHASE_MENU);
    assert(a.menu == SAND_KIND_STOPWATCH);
    assert(a.elapsed_ms == 0);
    for (int i = 0; i < SAND_GRAIN_COUNT; i++) assert(!a.grains[i].alive);
}

static void pump(sand_timer_t *timer, int frames)
{
    for (int i = 0; i < frames; i++) sand_timer_tick(timer, 40);
}

static int overlap_area(const sand_grain_t *a, const sand_grain_t *b)
{
    int left = a->x > b->x ? a->x : b->x;
    int right = (a->x + a->size) < (b->x + b->size) ? (a->x + a->size) : (b->x + b->size);
    int top = a->y > b->y ? a->y : b->y;
    int bottom = (a->y + a->ly) < (b->y + b->ly) ? (a->y + a->ly) : (b->y + b->ly);
    if (right <= left || bottom <= top) return 0;
    return (right - left) * (bottom - top);
}

static void test_forms_vary(void)
{
    sand_timer_t timer;
    char name[16];
    static const char *shape_name[SAND_CHOICE_COUNT] = {
        "CIRCLE", "SQUARE", "TRIANGLE", "DIAMOND", "HEXAGON", "RECTANGLE"
    };
    static const char *choice_name[SAND_CHOICE_COUNT] = {
        "Circle", "Square", "Triangle", "Diamond", "Hexagon", "Rectangle"
    };
    static const char *material_name[SAND_MATERIAL_COUNT] = {
        "Fire", "Water", "Stone", "Yuan", "Wood"
    };

    sand_timer_init(&timer);
    assert(timer.light_level == 3);
    assert(timer.sound_level == 2);
    assert(timer.dial == 0);
    assert(timer.color == 0);
    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    assert(timer.light_level == 4);
    assert(timer.dial == 4);
    assert(timer.color == 0);
    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    assert(timer.light_level == 1);
    assert(timer.dial == 1);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN_LONG);
    assert(timer.sound_level == 3);
    assert(timer.dial == 3);
    assert(timer.menu == SAND_KIND_FORM);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN_LONG);
    assert(timer.sound_level == 1);
    assert(sand_light_percent(1) == 20);
    assert(sand_light_percent(3) == 60);
    assert(sand_light_percent(5) == 100);
    assert(sand_sound_percent(1) == 30);
    assert(sand_sound_percent(2) == 55);
    assert(sand_sound_percent(3) == 85);
    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    assert(timer.dial == 2);
    sand_timer_tick(&timer, SAND_DIAL_MS - 1);
    assert(timer.dial == 2);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN_LONG);
    assert(timer.dial == 2);
    assert(timer.sound_level == 2);
    sand_timer_tick(&timer, SAND_DIAL_MS - 1);
    assert(timer.dial == 2);
    sand_timer_tick(&timer, 1);
    assert(timer.dial == 0);
    assert(timer.light_level == 2);
    enter_form(&timer);
    assert(timer.setup_row == SAND_ROW_START);
    assert(timer.form_choice == SAND_CHOICE_DIAMOND);
    assert(timer.material == SAND_MAT_YUAN);
    assert(sand_form_time_min(timer.form_time) == 20);
    assert(timer.duration_ms == 20u * 60u * 1000u);
    assert(sand_form_time_ms(0) == 60u * 1000u);
    assert(sand_form_time_min(SAND_FORM_TIME_COUNT - 1) == 60);
    assert(sand_form_time_ms(SAND_FORM_TIME_COUNT - 1) == 60u * 60u * 1000u);
    sand_timer_on_input(&timer, SAND_INPUT_UP);
    sand_timer_on_input(&timer, SAND_INPUT_UP);
    sand_timer_on_input(&timer, SAND_INPUT_UP);
    assert(timer.setup_row == SAND_ROW_TIME);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.form_time == 4);
    assert(sand_form_time_min(timer.form_time) == 30);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.form_time == 1);
    assert(sand_form_time_min(timer.form_time) == 5);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_ROW_SHAPE);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.form_choice == SAND_CHOICE_SQUARE);
    sand_choice_name(timer.form_choice, name, sizeof(name));
    assert(strcmp(name, "Square") == 0);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_ROW_COLOR);
    for (int step = 0; step < 3; step++) {
        sand_timer_on_input(&timer, SAND_INPUT_OK);
    }
    assert(timer.material == SAND_MAT_WATER);
    sand_material_name(timer.material, name, sizeof(name));
    assert(strcmp(name, "Water") == 0);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_ROW_START);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.phase == SAND_PHASE_RUNNING);
    assert(timer.form == SAND_FORM_SQUARE);
    {
        int min_x = SAND_SCREEN_W;
        int max_x = 0;
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            assert(!timer.grains[i].alive);
            assert(timer.grains[i].y + timer.grains[i].ly <= 0);
            if (timer.grains[i].x < min_x) min_x = timer.grains[i].x;
            if (timer.grains[i].x > max_x) max_x = timer.grains[i].x;
        }
        assert(max_x - min_x > 80);
    }
    pump(&timer, 120);
    {
        int falling = 0;
        int kept = -1;
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (!timer.grains[i].alive) continue;
            falling++;
            if (kept < 0) kept = i;
        }
        assert(falling > 0);
        assert(kept >= 0);
        assert(timer.grains[kept].tone == SAND_CHOICE_SQUARE);
        assert(timer.grains[kept].lz == SAND_MAT_WATER);
        int kept_spin = timer.grains[kept].spin;
        sand_timer_on_input(&timer, SAND_INPUT_UP);
        sand_timer_on_input(&timer, SAND_INPUT_DOWN);
        assert(timer.grains[kept].tone == SAND_CHOICE_SQUARE);
        assert(timer.grains[kept].lz == SAND_MAT_WATER);
        assert(timer.grains[kept].spin == kept_spin);
    }
    assert(timer.form_choice == SAND_CHOICE_TRIANGLE);
    assert(timer.material == SAND_MAT_STONE);
    sand_form_name(&timer, name, sizeof(name));
    assert(strcmp(name, "TRIANGLE") == 0);

    for (int choice = 0; choice < SAND_CHOICE_COUNT; choice++) {
        start_form_choice(&timer, choice, 0xA5A5u + (uint32_t)choice * 17u);
        sand_form_name(&timer, name, sizeof(name));
        assert(strcmp(name, shape_name[choice]) == 0);
        sand_choice_name(choice, name, sizeof(name));
        assert(strcmp(name, choice_name[choice]) == 0);
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            assert(timer.grains[i].y + timer.grains[i].ly <= 0);
        }
    }
    {
        int minute_release = 0;
        int long_release = 0;
        int turns = 0;
        unsigned seen = 0;
        start_form_choice(&timer, SAND_CHOICE_CIRCLE, 0x11u);
        assert(timer.grains[0].speed_px_per_s >= 36);
        assert(timer.grains[0].speed_px_per_s <= 96);
        assert(timer.grains[0].size <= 16);
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (timer.grains[i].start_x > minute_release) minute_release = timer.grains[i].start_x;
        }
        pump(&timer, 80);
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (!timer.grains[i].alive) continue;
            seen |= 1u << (timer.grains[i].spin & 7);
            assert(timer.grains[i].size <= 20);
        }
        for (int bit = 0; bit < 8; bit++) if (seen & (1u << bit)) turns++;
        assert(turns >= 3);
        sand_timer_init(&timer);
        sand_timer_set_entropy(&timer, 0x11u);
        enter_form(&timer);
        timer.form_time = 2;
        timer.setup_row = SAND_ROW_START;
        sand_timer_on_input(&timer, SAND_INPUT_OK);
        assert(timer.duration_ms == 10u * 60u * 1000u);
        assert(timer.grains[0].speed_px_per_s >= 36);
        assert(timer.grains[0].speed_px_per_s <= 96);
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (timer.grains[i].start_x > long_release) long_release = timer.grains[i].start_x;
        }
        assert(minute_release > 40 * 1000);
        assert(minute_release < 60 * 1000);
        assert(long_release > 8 * 60 * 1000);
    }
    for (int choice = 0; choice < SAND_CHOICE_COUNT; choice++) {
        int settled = 0;
        int top = SAND_SCREEN_H;
        int min_x = SAND_SCREEN_W;
        int max_x = 0;
        int worst = 0;
        start_form_choice(&timer, choice, 0x1234u);
        pump(&timer, 1500);
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (!timer.grains[i].mark) continue;
            settled++;
            if (timer.grains[i].y < top) top = timer.grains[i].y;
            if (timer.grains[i].x < min_x) min_x = timer.grains[i].x;
            if (timer.grains[i].x > max_x) max_x = timer.grains[i].x;
            for (int j = i + 1; j < SAND_PILE_COUNT; j++) {
                int area;
                if (!timer.grains[j].mark) continue;
                area = overlap_area(&timer.grains[i], &timer.grains[j]);
                if (area > worst) worst = area;
            }
        }
        assert(worst < 24);
        assert(settled + 4 >= SAND_PILE_COUNT);
        assert(top <= -4);
        assert(top >= -36);
        assert(max_x - min_x > 80);
    }
    sand_timer_init(&timer);
    sand_timer_set_entropy(&timer, 0x1234u);
    enter_form(&timer);
    timer.form_time = 2;
    timer.duration_ms = sand_form_time_ms(timer.form_time);
    timer.setup_row = SAND_ROW_START;
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.duration_ms == 10u * 60u * 1000u);
    pump(&timer, 1500);
    {
        int top = SAND_SCREEN_H;
        int released = 0;
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (!timer.grains[i].alive) continue;
            released++;
            if (timer.grains[i].mark && timer.grains[i].y < top) top = timer.grains[i].y;
        }
        assert(released < SAND_PILE_COUNT / 4);
        assert(top > 140);
    }
    for (int material = 0; material < SAND_MATERIAL_COUNT; material++) {
        sand_material_name(material, name, sizeof(name));
        assert(strcmp(name, material_name[material]) == 0);
        assert(sand_material_rgb(material, 1) != 0);
        for (int other = material + 1; other < SAND_MATERIAL_COUNT; other++) {
            assert(sand_material_rgb(material, 1) != sand_material_rgb(other, 1));
        }
    }
    sand_timer_init(&timer);
    enter_form(&timer);
    assert(timer.setup_row == SAND_ROW_START);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.phase == SAND_PHASE_RUNNING);
    assert(timer.form == SAND_FORM_DIAMOND);
    assert(timer.material == SAND_MAT_YUAN);
    assert(timer.duration_ms == 20u * 60u * 1000u);
    sand_timer_on_input(&timer, SAND_INPUT_OK_LONG);
    assert(timer.phase == SAND_PHASE_SETUP);
    assert(timer.setup_row == SAND_ROW_START);
    sand_timer_on_input(&timer, SAND_INPUT_UP_LONG);
    assert(timer.material == SAND_MAT_WOOD);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN_LONG);
    assert(timer.material == SAND_MAT_WOOD);
    assert(timer.sound_level == 2);

    sand_timer_init(&timer);
    while (timer.menu != SAND_KIND_LETTER) sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.kind == SAND_KIND_LETTER);
    assert(timer.letter == 0);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.letter == 1);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_LROW_TIME);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_LROW_COLOR);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.color == 1);
    sand_timer_on_input(&timer, SAND_INPUT_DOWN);
    assert(timer.setup_row == SAND_LROW_START);
    sand_timer_on_input(&timer, SAND_INPUT_OK);
    assert(timer.phase == SAND_PHASE_RUNNING);
    assert(timer.form == SAND_FORM_LETTER);
    sand_form_name(&timer, name, sizeof(name));
    assert(strcmp(name, "LETTER B") == 0);
    for (int i = 0; i < SAND_PILE_COUNT; i++) assert(timer.grains[i].y + timer.grains[i].ly <= 0);
    pump(&timer, 80);
    {
        int kept = -1;
        for (int i = 0; i < SAND_PILE_COUNT; i++) {
            if (!timer.grains[i].alive) continue;
            kept = i;
            break;
        }
        assert(kept >= 0);
        assert(timer.grains[kept].tone == 1);
        assert(timer.grains[kept].lz == 1);
        int kept_spin = timer.grains[kept].spin;
        sand_timer_on_input(&timer, SAND_INPUT_UP);
        sand_timer_on_input(&timer, SAND_INPUT_DOWN);
        assert(timer.letter == 2);
        assert(timer.color == 2);
        assert(timer.grains[kept].tone == 1);
        assert(timer.grains[kept].lz == 1);
        assert(timer.grains[kept].spin == kept_spin);
    }
    pump(&timer, 320);
    {
        int settled = 0;
        for (int i = 0; i < SAND_PILE_COUNT; i++) if (timer.grains[i].mark) settled++;
        assert(settled >= 2);
    }
}

int main(void)
{
    test_presets_and_fill();
    test_pick_and_clock();
    test_run_pause_color_and_finish();
    test_menu_and_stopwatch();
    test_forms_vary();
    return 0;
}

// 沙漏计时。屏幕、按键和电量走 BSP；页面和计时模型在 main。
//
// 目录：Form 在最上，其下是 Letter、Pomodoro、Stopwatch。上/下选择，确定进入，长按上键换色。
// Form：小小的几何粒带重力随机落下，方向不固定，堆到画面顶时倒计时刚好结束。换形状或材质只影响新落下的粒。
// Letter：小粗体字母同样落下。换字母或颜色只影响新落下的粒。结束时一声短音，画面中央以原始字号正向计时。
// Pomodoro：再选 10、20、30 分钟。沙粒往下掉并堆积，时间到后响一声并正向计时。
// Stopwatch：左上角从 00:00 正向计时。沙粒只在时间下方往上升，不盖住文字，也不堆积。
//           确定暂停或继续，长按确定回到目录。
// 非计时状态下 45 秒无操作会压低背光。计时期间保持点亮，不进入睡眠。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "sand_timer.h"
#include "sand_ui.h"
#include "sand_chime.h"

#include <stdbool.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "sand";

#define INPUT_QUEUE_DEPTH 8
#define FRAME_MS 16
#define IDLE_DIM_US (45LL * 1000 * 1000)
#define BATTERY_PERIOD_US (5LL * 1000 * 1000)
#define BACKLIGHT_ACTIVE 68
#define BACKLIGHT_IDLE 12

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static sand_timer_t s_timer;

static bool map_input(bsp_btn_t btn, bsp_btn_ev_t event, sand_input_t *input)
{
    if (event == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        *input = SAND_INPUT_OK_LONG;
        return true;
    }
    if (event == BSP_BTN_LONG && btn == BSP_BTN_UP) {
        *input = SAND_INPUT_UP_LONG;
        return true;
    }
    if (event != BSP_BTN_CLICK) return false;
    if (btn == BSP_BTN_UP) *input = SAND_INPUT_UP;
    else if (btn == BSP_BTN_DOWN) *input = SAND_INPUT_DOWN;
    else if (btn == BSP_BTN_OK) *input = SAND_INPUT_OK;
    else return false;
    return true;
}

static void app_task(void *arg)
{
    (void)arg;
    int64_t last = esp_timer_get_time();
    int64_t last_input = last;
    int64_t last_battery = 0;
    int battery = -1;
    uint8_t backlight = 0xFF;
    sand_phase_t previous = SAND_PHASE_MENU;

    for (;;) {
        input_event_t input;
        int64_t now = esp_timer_get_time();
        int wait_ms = FRAME_MS - (int)((now - last) / 1000);
        if (wait_ms < 1) wait_ms = 1;
        bool got = xQueueReceive(s_input_queue, &input, pdMS_TO_TICKS(wait_ms)) == pdTRUE;
        now = esp_timer_get_time();
        int64_t delta = now - last;
        last = now;
        if (delta < 0) delta = 0;
        if (delta > 300000000LL) delta = 300000000LL;
        uint32_t dt_ms = (uint32_t)(delta / 1000);

        if (got) {
            last_input = now;
            sand_input_t mapped;
            if (map_input(input.btn, input.event, &mapped)) {
                if (mapped == SAND_INPUT_OK) {
                    sand_timer_set_entropy(&s_timer, (uint32_t)now);
                }
                sand_timer_on_input(&s_timer, mapped);
            }
        }
        sand_timer_tick(&s_timer, dt_ms);
        if (s_timer.phase == SAND_PHASE_DONE && previous != SAND_PHASE_DONE) {
            sand_chime_play();
        }
        previous = s_timer.phase;

        if (last_battery == 0 || now - last_battery >= BATTERY_PERIOD_US) {
            battery = bsp_battery_soc();
            last_battery = now;
        }

        uint8_t level = BACKLIGHT_ACTIVE;
        if (s_timer.phase != SAND_PHASE_RUNNING && now - last_input >= IDLE_DIM_US) {
            level = BACKLIGHT_IDLE;
        }
        if (level != backlight) {
            bsp_display_backlight(level);
            backlight = level;
        }

        if (bsp_lvgl_lock(200)) {
            sand_ui_sync(&s_timer, battery);
            bsp_lvgl_unlock();
        }
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = {.btn = btn, .event = event};
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "sand timer start");
    sand_timer_init(&s_timer);

    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display init failed MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(BACKLIGHT_ACTIVE);

    if (bsp_battery_init() != ESP_OK) {
        ESP_LOGW(TAG, "battery gauge unavailable");
    }
    sand_chime_start();

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "input queue create failed");
        return;
    }
    if (xTaskCreate(app_task, "sand_app", 8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "ui task create failed");
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }
    s_input_ready = true;
}

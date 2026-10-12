#include "sand_chime.h"

#include "bsp_audio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sand_chime";

#define CHIME_RATE 16000
#define CHIME_CHUNK 256
#define CHIME_TOTAL_MS 150

static TaskHandle_t s_task;

static bool write_samples(const int16_t *pcm, int count)
{
    return bsp_audio_write(pcm, (size_t)count * sizeof(int16_t)) == ESP_OK;
}

static bool write_tone(int hz, int ms)
{
    int16_t buf[CHIME_CHUNK];
    int period = CHIME_RATE / hz;
    if (period < 2) period = 2;
    int total = CHIME_RATE * ms / 1000;
    int phase = 0;
    while (total > 0) {
        int n = total < CHIME_CHUNK ? total : CHIME_CHUNK;
        for (int i = 0; i < n; i++) {
            buf[i] = (phase < period / 2) ? 4500 : -4500;
            if (++phase >= period) phase = 0;
        }
        if (!write_samples(buf, n)) return false;
        total -= n;
    }
    return true;
}

static void play_once(void)
{
    if (bsp_audio_init() != ESP_OK) {
        ESP_LOGW(TAG, "audio init failed");
        return;
    }
    if (bsp_audio_set_format(CHIME_RATE, 16, 1) != ESP_OK) {
        ESP_LOGW(TAG, "audio format failed");
        return;
    }
    bsp_audio_set_volume(55);
    /* One short pip. */
    if (!write_tone(1320, CHIME_TOTAL_MS)) return;
}

static void chime_task(void *arg)
{
    (void)arg;
    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        play_once();
    }
}

void sand_chime_start(void)
{
    if (s_task) return;
    if (xTaskCreate(chime_task, "sand_chime", 4096, NULL, 4, &s_task) != pdPASS) {
        s_task = NULL;
        ESP_LOGW(TAG, "chime task create failed");
    }
}

void sand_chime_play(void)
{
    if (s_task) xTaskNotifyGive(s_task);
}

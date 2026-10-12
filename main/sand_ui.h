#pragma once

#include "sand_timer.h"

/* Rebuilds or updates the sand-timer screens. Caller must already hold the LVGL lock. */
void sand_ui_sync(const sand_timer_t *timer, int battery_soc);

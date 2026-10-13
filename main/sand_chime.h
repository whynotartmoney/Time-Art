#pragma once

#include <stdint.h>

/* Starts the one-shot completion chime task. Playback stays off the UI task. */
void sand_chime_start(void);

/* Asks the chime task to play. Safe from the app task; does not block. */
void sand_chime_play(void);

/* Percent used the next time the pip plays. Values above 100 are clamped. */
void sand_chime_set_volume(uint8_t percent);

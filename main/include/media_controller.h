#ifndef MEDIA_CONTROLLER_H
#define MEDIA_CONTROLLER_H

#include "lvgl/lvgl.h"

#define STREAMPLAYER_KEY_SUBTITLE 0xF100u
#define STREAMPLAYER_KEY_CTRL_LEFT 0xF101u
#define STREAMPLAYER_KEY_CTRL_RIGHT 0xF102u

#ifdef __cplusplus
extern "C" {
#endif

void media_controller_init(void);
void media_controller_attach_keyboard(lv_indev_t *preferred);
void media_controller_shutdown(void);
/* Called on the LVGL main thread before keypad navigation consumes Tab.
 * Returns nonzero only on the playback screen; text-entry navigation stays
 * with LVGL. A release/repeat is consumed without switching output again. */
int media_controller_intercept_tab(int pressed);

#ifdef __cplusplus
}
#endif

#endif

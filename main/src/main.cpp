#include "lvgl/lvgl.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include "ui/ui.h"
#include "keyboard_input.h"
#include "media_controller.h"
#if LV_USE_EVDEV
#include <linux/input.h>
#endif
#include <cstring>

static lv_indev_t *g_keyboard_indev = NULL;
static volatile sig_atomic_t g_shutdown_requested = 0;

static void request_shutdown(int signal_number)
{
    (void)signal_number;
    g_shutdown_requested = 1;
}

static const char *getenv_default(const char *name, const char *dflt)
{
    return getenv(name) ?: dflt;
}

int get_cardputer_fbdev(char *dev_path, size_t buf_size)
{
    if (dev_path == NULL || buf_size == 0)
    {
        return -1;
    }

    FILE *fp = fopen("/proc/fb", "r");
    if (fp == NULL)
    {
        perror("Failed to open /proc/fb");
        return -1;
    }

    char line[256];
    int preferred_fb_num = -1;
    int fallback_fb_num = -1;
    int framebuffer_count = 0;

    /*
     * The legacy image reports "fb_st7789v"; newer CardputerZero images use
     * "panel-mipi-dbid". Prefer either known LCD driver, and only fall back
     * to an unnamed framebuffer when the system exposes exactly one.
     */
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        int fb_num = -1;
        char driver_name[128] = {0};
        if (sscanf(line, "%d %127s", &fb_num, driver_name) != 2)
        {
            continue;
        }

        if (framebuffer_count == 0)
        {
            fallback_fb_num = fb_num;
        }
        framebuffer_count++;

        if (strcmp(driver_name, "fb_st7789v") == 0 ||
            strcmp(driver_name, "panel-mipi-dbid") == 0)
        {
            preferred_fb_num = fb_num;
            break;
        }
    }

    fclose(fp);

    int fb_num = preferred_fb_num;
    if (fb_num < 0 && framebuffer_count == 1)
    {
        fb_num = fallback_fb_num;
    }
    if (fb_num < 0)
    {
        fprintf(stderr, "CardputerZero framebuffer not found in /proc/fb\n");
        return -1;
    }

    snprintf(dev_path, buf_size, "/dev/fb%d", fb_num);
    return 0;
}


#if LV_USE_EVDEV

static uint32_t evdev_key_to_lvgl(const struct key_item *key)
{
    if (!key) return 0;

    if ((key->mods & KBD_MOD_CTRL) && key->key_code == KEY_LEFT)
        return STREAMPLAYER_KEY_CTRL_LEFT;
    if ((key->mods & KBD_MOD_CTRL) && key->key_code == KEY_RIGHT)
        return STREAMPLAYER_KEY_CTRL_RIGHT;

    switch (key->key_code)
    {
    case KEY_UP:
        return LV_KEY_UP;
    case KEY_DOWN:
        return LV_KEY_DOWN;
    case KEY_RIGHT:
        return LV_KEY_RIGHT;
    case KEY_LEFT:
        return LV_KEY_LEFT;
    case KEY_ESC:
        return LV_KEY_ESC;
    case KEY_DELETE:
        return LV_KEY_DEL;
    case KEY_BACKSPACE:
        return LV_KEY_BACKSPACE;
    case KEY_ENTER:
        return LV_KEY_ENTER;
    case KEY_NEXT:
    case KEY_PAGEDOWN:
        return LV_KEY_NEXT;
    case KEY_PREVIOUS:
    case KEY_PAGEUP:
        return LV_KEY_PREV;
    case KEY_HOME:
        return LV_KEY_HOME;
    case KEY_END:
        return LV_KEY_END;
    case KEY_TAB:
        return LV_KEY_NEXT;
    default:
        break;
    }

    if (key->codepoint) return key->codepoint;
    if (key->utf8[0] && key->utf8[1] == '\0') return (uint8_t)key->utf8[0];
    return key->key_code;
}

static void keypad_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;

    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;

    pthread_mutex_lock(&keyboard_mutex);
    if (!STAILQ_EMPTY(&keyboard_queue))
    {
        struct key_item *key = STAILQ_FIRST(&keyboard_queue);
        STAILQ_REMOVE_HEAD(&keyboard_queue, entries);

        data->key = evdev_key_to_lvgl(key);
        if (data->key)
        {
            data->state = key->key_state == KBD_KEY_RELEASED
                              ? LV_INDEV_STATE_RELEASED
                              : LV_INDEV_STATE_PRESSED;
            data->continue_reading = !STAILQ_EMPTY(&keyboard_queue);
        }

        printf("StreamPlayer key: code=%u state=%d sym=%s utf8=%s lv=%u\n",
               key->key_code, key->key_state, key->sym_name, key->utf8, data->key);
        free(key);
    }
    pthread_mutex_unlock(&keyboard_mutex);
}

static void lv_linux_indev_init(void)
{
    const char *mouse_device = getenv_default("LV_LINUX_MOUSE_DEVICE", NULL);
    const char *keyboard_device = getenv_default("LV_LINUX_KEYBOARD_DEVICE", "/dev/input/by-path/platform-3f804000.i2c-event");
    const char *keyboard_map = getenv_default("LV_LINUX_KEYBOARD_MAP", "/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map");
    // /home/nihao/w2T/github/m5stack-linux-dtoverlays/modules/tca8418-1.0/tca8418_keypad_m5stack_keymap.map


    lv_indev_t *touch = NULL;
    if (mouse_device)
        touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, mouse_device);

    (void)touch;
    (void)keyboard_map;
    if (keyboard_device)
    {
        pthread_t keyboard_read_thread_id;
        pthread_create(&keyboard_read_thread_id,
                       NULL,
                       keyboard_read_thread,
                       (void *)keyboard_device);
        pthread_detach(keyboard_read_thread_id);

        g_keyboard_indev = lv_indev_create();
        lv_indev_set_type(g_keyboard_indev, LV_INDEV_TYPE_KEYPAD);
        lv_indev_set_read_cb(g_keyboard_indev, keypad_read_cb);
    }
}
#endif

#if LV_USE_LINUX_FBDEV
static void lv_linux_disp_init(void)
{
    // export LV_LINUX_FBDEV_DEVICE="/dev/fb$(grep 'fb_st7789v' /proc/fb | awk '{print $1}')"
    const char *device = NULL;
    char fbdev[64] = {0};
    device = getenv_default("LV_LINUX_FBDEV_DEVICE", NULL);
    if ((device == NULL) && (get_cardputer_fbdev(fbdev, sizeof(fbdev)) == 0))
    {
        device = fbdev;
    }
    if (device == NULL)
    {
        fprintf(stderr, "No framebuffer device available\n");
        return;
    }
    printf("Using framebuffer device: %s\n", device);
    lv_display_t *disp = lv_linux_fbdev_create();
    if (disp == NULL)
    {
        printf("Failed to create fbdev display!\n");
        return;
    }

    lv_linux_fbdev_set_file(disp, device);

    // 打印获取到的分辨率
    lv_coord_t w = lv_display_get_horizontal_resolution(disp);
    lv_coord_t h = lv_display_get_vertical_resolution(disp);
    printf("Framebuffer resolution: %dx%d\n", w, h);
}
#if !LV_USE_EVDEV && !LV_USE_LIBINPUT
static void lv_linux_indev_init(void)
{
}
#endif

#elif LV_USE_LINUX_DRM
static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_DRM_CARD", "/dev/dri/card0");
    lv_display_t *disp = lv_linux_drm_create();

    lv_linux_drm_set_file(disp, device, -1);
}
#elif LV_USE_SDL
static void lv_linux_disp_init(void)
{
    const int width = atoi(getenv("LV_SDL_VIDEO_WIDTH") ?: "320");
    const int height = atoi(getenv("LV_SDL_VIDEO_HEIGHT") ?: "170");

    lv_sdl_window_create(width, height);
}

static void lv_linux_indev_init(void)
{
    lv_sdl_mouse_create();
    g_keyboard_indev = lv_sdl_keyboard_create();
}

#else
#error Unsupported configuration
#endif

int main(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = request_shutdown;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);

    lv_init();

    /*Linux display device init*/
    lv_linux_disp_init();

    lv_linux_indev_init();
    /*Create a Demo*/
    // lv_demo_widgets();
    // lv_demo_widgets_start_slideshow();
    // lv_demo_music();

    ui_init();
    media_controller_init();
    media_controller_attach_keyboard(g_keyboard_indev);
    // lv_demo_widgets(); // 用LVGL自带demo测试
    /*Handle LVGL tasks*/
    printf("Entering main loop...\n");
    while (!g_shutdown_requested)
    {
        lv_timer_handler();
        usleep(1000);
    }

    media_controller_shutdown();
    return 0;
}

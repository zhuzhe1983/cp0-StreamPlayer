/* Exercise the production decoder and queue with real xkbcommon. This test
 * never opens an input device or reads user keystrokes. */
#include "../main/src/keyboard_input.c"
#include <assert.h>

static void expect_event(struct kbd_ctx *kc, uint32_t code, int down,
                         uint32_t expected)
{
    process_key(kc, code, down);
    assert(!STAILQ_EMPTY(&keyboard_queue));
    struct key_item *item = STAILQ_FIRST(&keyboard_queue);
    STAILQ_REMOVE_HEAD(&keyboard_queue, entries);
    assert(item->key_code == code);
    assert(item->key_state == (down ? KBD_KEY_PRESSED : KBD_KEY_RELEASED));
    assert(item->codepoint == expected);
    free(item);
    assert(STAILQ_EMPTY(&keyboard_queue));
}

int main(int argc, char **argv)
{
    assert(argc == 2);
    STAILQ_INIT(&keyboard_queue);
    struct kbd_ctx kc = {0};
    assert(init_xkb(&kc, "us", "C.UTF-8") == 0);
    kc.repeat_fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    assert(kc.repeat_fd >= 0);
    kc.use_tca_keymap = true;
    kc.runtime_keymap_loaded = tca8418_runtime_keymap_load(&kc.runtime_keymap, argv[1]) > 0;
    assert(kc.runtime_keymap_loaded);
    const unsigned int codes[] = {
        26,27,39,40,41,43,51,52,53,94,55,69,70,71,72,73,
        74,75,76,77,79,80,81,82,83,85,86,89,90,91,92,93
    };
    const char expected[] = "!@#$%^&*()~`_-+=[]{};:'\"<>\\|,./?";
    assert(sizeof(expected)-1 == sizeof(codes)/sizeof(codes[0]));
    for (size_t i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i) {
        expect_event(&kc, codes[i], 1, (unsigned char)expected[i]);
        expect_event(&kc, codes[i], 0, (unsigned char)expected[i]);
    }
    expect_event(&kc, KEY_A, 1, 'a'); expect_event(&kc, KEY_A, 0, 'a');
    expect_event(&kc, KEY_B, 1, 'b'); expect_event(&kc, KEY_B, 0, 'b');
    expect_event(&kc, KEY_LEFTSHIFT, 1, 0);
    expect_event(&kc, KEY_A, 1, 'A'); expect_event(&kc, KEY_A, 0, 'A');
    expect_event(&kc, KEY_LEFTSHIFT, 0, 0);
    expect_event(&kc, KEY_A, 1, 'a'); expect_event(&kc, KEY_A, 0, 'a');
    expect_event(&kc, KEY_BACKSPACE, 1, '\b');
    expect_event(&kc, KEY_BACKSPACE, 0, '\b');
    expect_event(&kc, KEY_TAB, 1, '\t'); expect_event(&kc, KEY_TAB, 0, '\t');
    /* No loaded map: the legacy built-in symbols still work. */
    kc.runtime_keymap_loaded = false;
    expect_event(&kc, 184, 1, '@'); expect_event(&kc, 184, 0, '@');
    expect_event(&kc, 205, 1, ':'); expect_event(&kc, 205, 0, ':');
    /* A USB keyboard must use standard xkb keys even when a TCA map exists. */
    kc.runtime_keymap_loaded = true;
    kc.use_tca_keymap = false;
    expect_event(&kc, KEY_LEFTBRACE, 1, '[');
    expect_event(&kc, KEY_LEFTBRACE, 0, '[');
    close(kc.repeat_fd);
    free_xkb(&kc);
    puts("production keyboard translation: all 32 symbols, press/release, a/b/Shift-A, backspace/tab, legacy fallback and USB isolation passed");
    return 0;
}

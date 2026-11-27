#ifndef STREAMPLAYER_KEYBOARD_KEYMAP_H
#define STREAMPLAYER_KEYBOARD_KEYMAP_H

#include <stddef.h>
#include <stdint.h>

/* Linux input key codes, including the legacy TCA8418 symbol range. */
#define TCA8418_RUNTIME_KEY_COUNT 768
#define TCA8418_RUNTIME_SYMBOL_SIZE 32

struct tca8418_runtime_keymap {
    char symbols[TCA8418_RUNTIME_KEY_COUNT][TCA8418_RUNTIME_SYMBOL_SIZE];
};

/* Read the unmodified image's "keycode N = keysym" entries. No loadkeys,
 * shell commands, or system keymap changes are needed. Returns the number
 * of distinct usable entries, or -1 when the file cannot be read. */
int tca8418_runtime_keymap_load(struct tca8418_runtime_keymap *map,
                              const char *path);
const char *tca8418_runtime_keymap_lookup(
    const struct tca8418_runtime_keymap *map, uint32_t code);

#endif

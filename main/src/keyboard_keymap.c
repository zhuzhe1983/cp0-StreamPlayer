#include "keyboard_keymap.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

int tca8418_runtime_keymap_load(struct tca8418_runtime_keymap *map,
                              const char *path)
{
    if (!map) return -1;
    memset(map, 0, sizeof(*map));
    if (!path || !*path) return -1;
    FILE *file = fopen(path, "r");
    if (!file) return -1;

    char line[256];
    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        /* Ignore a whole overlong line rather than accepting its prefix. */
        if (!strchr(line, '\n') && !feof(file)) {
            int ch;
            while ((ch = fgetc(file)) != '\n' && ch != EOF) {}
            continue;
        }
        unsigned int code;
        char symbol[TCA8418_RUNTIME_SYMBOL_SIZE];
        int used = 0;
        if (sscanf(line, " keycode %u = %31[A-Za-z0-9_]%n",
                   &code, symbol, &used) != 2 || used == 0 ||
            code >= TCA8418_RUNTIME_KEY_COUNT) continue;
        const char *tail = line + used;
        while (isspace((unsigned char)*tail)) ++tail;
        if (*tail && *tail != '#') continue;
        if (!map->symbols[code][0]) ++count;
        snprintf(map->symbols[code], sizeof(map->symbols[code]), "%s", symbol);
    }
    if (ferror(file)) {
        memset(map, 0, sizeof(*map));
        count = -1;
    }
    fclose(file);
    return count;
}

const char *tca8418_runtime_keymap_lookup(
    const struct tca8418_runtime_keymap *map, uint32_t code)
{
    if (!map || code >= TCA8418_RUNTIME_KEY_COUNT || !map->symbols[code][0])
        return NULL;
    return map->symbols[code];
}

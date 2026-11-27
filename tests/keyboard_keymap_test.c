#define _POSIX_C_SOURCE 200809L
#include "keyboard_keymap.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    struct tca8418_runtime_keymap map;
    assert(tca8418_runtime_keymap_load(&map, argv[1]) == 32);
    const unsigned int codes[] = {
        26,27,39,40,41,43,51,52,53,94,55,69,70,71,72,73,
        74,75,76,77,79,80,81,82,83,85,86,89,90,91,92,93
    };
    const char *symbols[] = {
        "exclam","at","numbersign","dollar","percent","asciicircum",
        "ampersand","asterisk","parenleft","parenright","asciitilde",
        "grave","underscore","minus","plus","equal","bracketleft",
        "bracketright","braceleft","braceright","semicolon","colon",
        "apostrophe","quotedbl","less","greater","backslash","bar",
        "comma","period","slash","question"
    };
    for (size_t i = 0; i < sizeof(codes)/sizeof(codes[0]); ++i)
        assert(strcmp(tca8418_runtime_keymap_lookup(&map, codes[i]), symbols[i]) == 0);
    /* Normal letters, modifiers and navigation stay with xkbcommon. Old
     * symbol codes must not leak into a successfully loaded new layout. */
    const unsigned int unmapped[] = {1,14,15,28,30,42,54,103,105,106,108,183,205,213};
    for (size_t i = 0; i < sizeof(unmapped)/sizeof(unmapped[0]); ++i)
        assert(tca8418_runtime_keymap_lookup(&map, unmapped[i]) == NULL);
    assert(tca8418_runtime_keymap_lookup(&map, UINT32_MAX) == NULL);

    char path[] = "/tmp/streamplayer-keymap-test.XXXXXX";
    int fd = mkstemp(path);
    assert(fd >= 0);
    FILE *file = fdopen(fd, "w");
    assert(file);
    fputs("# legacy image, tabs and CRLF accepted\n"
          " keycode 184 = at # legacy @\r\n"
          "\tkeycode 205 = colon\n"
          "keycode 197 = slash\n"
          "keycode 205 = semicolon # duplicate replaces value\n"
          "keycode 768 = bad\nkeycode -1 = bad\n"
          "keycode 2 = a b\nkeycode 3 = abc;system\n"
          "keycode 4 = abc-def\nkeycode 5 =\n"
          "alt keycode 6 = x\n", file);
    /* Ignore a truncated line completely. */
    fputs("keycode 7 = a #", file);
    for (int i = 0; i < 300; ++i) fputc('x', file);
    fputs("\nkeycode 92 = slash", file);
    assert(fclose(file) == 0);
    assert(tca8418_runtime_keymap_load(&map, path) == 4);
    assert(strcmp(tca8418_runtime_keymap_lookup(&map, 184), "at") == 0);
    assert(strcmp(tca8418_runtime_keymap_lookup(&map, 205), "semicolon") == 0);
    assert(strcmp(tca8418_runtime_keymap_lookup(&map, 197), "slash") == 0);
    assert(strcmp(tca8418_runtime_keymap_lookup(&map, 92), "slash") == 0);
    for (unsigned int i = 0; i <= 7; ++i)
        assert(tca8418_runtime_keymap_lookup(&map, i) == NULL);
    assert(unlink(path) == 0);
    assert(tca8418_runtime_keymap_load(&map, path) == -1);
    assert(tca8418_runtime_keymap_lookup(&map, 184) == NULL);
    assert(tca8418_runtime_keymap_load(&map, "") == -1);
    puts("keyboard keymap regression: 32 current symbols, legacy map, normal-key isolation, malformed/missing maps passed");
    return 0;
}

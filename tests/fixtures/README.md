`tca8418c-20261119.map` is the symbol-only console map shipped by the
2026-11-19 CardputerZero image (`tca8418c` input device). The regression
uses its real key codes; it contains no user input or account information.

Run the portable loader test with:

```
cc -std=c11 -Wall -Wextra -Werror -I main/include \
  main/src/keyboard_keymap.c tests/keyboard_keymap_test.c -o /tmp/keymap-test
/tmp/keymap-test tests/fixtures/tca8418c-20261119.map
```

`keyboard_translation_test.c` additionally includes the production decoder,
using real libinput/xkbcommon libraries but a display-free LVGL stub. Run
on a Linux host with those development libraries, or cross-compile for the
device, linking `-linput -lxkbcommon -lpthread`. It never opens an input
device or listens to user typing.
